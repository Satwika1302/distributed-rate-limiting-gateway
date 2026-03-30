# Order Management and Persistence Service
# This service handles e-commerce order data using an SQLite backend.
# It demonstrates stateful persistence in a microservices architecture.

from http.server import BaseHTTPRequestHandler, HTTPServer
import sys
import json
import sqlite3
import datetime
import re
import urllib.parse
import os
import random

port = int(sys.argv[1]) if len(sys.argv) > 1 else 9091
DB_NAME = "order_service_shared.db"

# Database initialization and seeding
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS orders 
                 (id TEXT PRIMARY KEY, user_id INTEGER, product TEXT, qty INTEGER, total REAL, status TEXT, created TEXT)''')
    
    # Seed data if empty
    c.execute("SELECT COUNT(*) FROM orders")
    if c.fetchone()[0] == 0:
        orders = [
            ("ORD-001", 1, "MacBook Pro M3",   1, 2499.00, "delivered", "2024-11-01"),
            ("ORD-002", 2, "Sony WH-1000XM5", 2, 698.00,  "shipped",   "2024-12-14"),
            ("ORD-003", 1, "iPad Air",        1, 799.00,  "confirmed", "2025-01-03"),
        ]
        c.executemany("INSERT INTO orders VALUES (?,?,?,?,?,?,?)", orders)
    conn.commit()
    conn.close()

def send_json(handler, status, data):
    body = json.dumps(data, indent=2).encode()
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json")
    handler.send_header("Content-Length", str(len(body)))
    handler.send_header("X-Served-By", f"order-service-{port}")

    # Read Gateway-injected headers
    uid = handler.headers.get("X-User-ID", "anonymous")
    role = handler.headers.get("X-User-Role", "guest")
    handler.send_header("X-Authorized-User", uid)
    handler.send_header("X-Authorized-Role", role)

    handler.end_headers()
    handler.wfile.write(body)

class OrderServiceHandler(BaseHTTPRequestHandler):
    def route(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        params = urllib.parse.parse_qs(parsed.query)
        conn = sqlite3.connect(DB_NAME)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # GET /api/orders → list all orders
            if path == "/api/orders" and self.command == "GET":
                uid = params.get("user_id", [None])[0]
                if uid:
                    c.execute("SELECT * FROM orders WHERE user_id = ?", (uid,))
                else:
                    c.execute("SELECT * FROM orders")
                rows = [dict(r) for r in c.fetchall()]
                send_json(self, 200, {"data": rows, "total": len(rows), "node": f"order-service:{port}"})

            # GET /api/orders/<id> → single order
            elif m := re.match(r"^/api/orders/(ORD-\d+)$", path):
                oid = m.group(1)
                c.execute("SELECT * FROM orders WHERE id = ?", (oid,))
                row = c.fetchone()
                if row:
                    send_json(self, 200, {"data": dict(row), "node": f"order-service:{port}"})
                else:
                    send_json(self, 404, {"error": f"Order {oid} not found"})

            # POST /api/orders → create order (PERSISTENT!)
            elif path == "/api/orders" and self.command == "POST":
                length = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(length) or '{}')
                order_id = f"ORD-{random.randint(100, 999)}"
                user_id = body.get("user_id", 1)
                product = body.get("product", "Unknown Item")
                qty = body.get("qty", 1)
                total = body.get("total", 0.0)
                status = "pending"
                created = datetime.date.today().isoformat()
                
                c.execute("INSERT INTO orders VALUES (?,?,?,?,?,?,?)", (order_id, user_id, product, qty, total, status, created))
                conn.commit()
                send_json(self, 201, {"data": {"id": order_id, "status": status}, "message": "Order saved to SQLite", "node": f"order-service:{port}"})

            # GET /api/health → health check
            elif path == "/api/health":
                send_json(self, 200, {
                    "status": "healthy",
                    "database": "sqlite3",
                    "orders_count": c.execute("SELECT COUNT(*) FROM orders").fetchone()[0],
                    "timestamp": datetime.datetime.utcnow().isoformat() + "Z"
                })

            else:
                send_json(self, 404, {"error": "Endpoint not found on order-service"})
        finally:
            conn.close()

    def do_GET(self):  self.route()
    def do_POST(self): self.route()
    def log_message(self, *_): pass

if __name__ == "__main__":
    init_db()
    server = HTTPServer(("0.0.0.0", port), OrderServiceHandler)
    print(f"✅ Persistent Order Service online on port {port} [DB: {DB_NAME}]")
    server.serve_forever()
