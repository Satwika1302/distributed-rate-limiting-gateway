# User Identity and Persistence Service
# This service handles user data management using an SQLite backend.
# It is designed to be proxied by the C++ API Gateway.

from http.server import BaseHTTPRequestHandler, HTTPServer
import sys
import json
import sqlite3
import datetime
import re
import urllib.parse
import os

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8081
DB_NAME = "user_service_shared.db"

# Database initialization and seeding
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS users 
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, 
                  name TEXT, email TEXT UNIQUE, role TEXT, joined TEXT)''')
    
    # Seed data if empty
    c.execute("SELECT COUNT(*) FROM users")
    if c.fetchone()[0] == 0:
        users = [
            ("Alice Carter",  "alice@example.com",  "admin",  "2022-03-01"),
            ("Bob Nguyen",    "bob@example.com",    "user",   "2022-07-14"),
            ("Clara Smith",   "clara@example.com",  "user",   "2023-01-22"),
            ("David Lee",     "david@example.com",  "editor", "2023-05-05"),
            ("Eva Rodriguez", "eva@example.com",    "user",   "2024-02-10"),
        ]
        c.executemany("INSERT INTO users (name, email, role, joined) VALUES (?,?,?,?)", users)
    conn.commit()
    conn.close()

def send_json(handler, status, data):
    # Identity Propagation Audit Log
    uid = handler.headers.get("X-User-ID", "anonymous")
    print(f"[AUDIT] Request authenticated for User: {uid} | Handled by: user-service-{port}")
    
    body = json.dumps(data, indent=2).encode()
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json")
    handler.send_header("X-Served-By", f"user-service-{port}")
    handler.send_header("X-Authorized-User", uid)
    handler.end_headers()
    handler.wfile.write(body)

class UserServiceHandler(BaseHTTPRequestHandler):
    def route(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        conn = sqlite3.connect(DB_NAME)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # GET /api/users → list all users
            if path == "/api/users" and self.command == "GET":
                c.execute("SELECT * FROM users")
                rows = [dict(r) for r in c.fetchall()]
                send_json(self, 200, {"data": rows, "total": len(rows), "node": f"user-service:{port}"})

            # GET /api/users/<id> → single user
            elif m := re.match(r"^/api/users/(\d+)$", path):
                uid = int(m.group(1))
                c.execute("SELECT * FROM users WHERE id = ?", (uid,))
                row = c.fetchone()
                if row:
                    send_json(self, 200, {"data": dict(row), "node": f"user-service:{port}"})
                else:
                    send_json(self, 404, {"error": f"User {uid} not found"})

            # POST /api/users → create user (PERSISTENT!)
            elif path == "/api/users" and self.command == "POST":
                length = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(length) or '{}')
                name = body.get("name", "New User")
                email = body.get("email", f"user{datetime.datetime.now().timestamp()}@example.com")
                role = "user"
                joined = datetime.date.today().isoformat()
                
                try:
                    c.execute("INSERT INTO users (name, email, role, joined) VALUES (?,?,?,?)", (name, email, role, joined))
                    conn.commit()
                    new_id = c.lastrowid
                    send_json(self, 201, {"data": {"id": new_id, "name": name, "email": email}, "message": "User saved to SQLite", "node": f"user-service:{port}"})
                except sqlite3.IntegrityError:
                    send_json(self, 400, {"error": "Email already exists"})

            # GET /api/health → health check
            elif path == "/api/health":
                send_json(self, 200, {
                    "status": "healthy",
                    "database": "sqlite3",
                    "db_file": DB_NAME,
                    "timestamp": datetime.datetime.utcnow().isoformat() + "Z"
                })

            else:
                send_json(self, 404, {"error": "Endpoint not found on user-service"})
        finally:
            conn.close()

    def do_GET(self):  self.route()
    def do_POST(self): self.route()
    def log_message(self, *_): pass

if __name__ == "__main__":
    init_db()
    server = HTTPServer(("0.0.0.0", port), UserServiceHandler)
    print(f"Persistent User Service online on port {port} [DB: {DB_NAME}]")
    server.serve_forever()
