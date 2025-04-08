import socket
import struct
import threading
import time
import random  # Import random for simulating packet loss

# Constants for message types
CONFIRM = 0x00
REPLY = 0x01
AUTH = 0x02
JOIN = 0x03
MSG = 0x04
PING = 0xFD
ERR = 0xFE
BYE = 0xFF

global client

# Server settings
HOST = "127.0.0.1"
INITIAL_PORT = 4567

# Configurable delay to simulate packet loss (in seconds)
PACKET_DELAY_MIN = 0.01
PACKET_DELAY_MAX = 0.26

# Global state
server_msg_id = 0
client = None  # Store information about the single connected client

def build_confirm(ref_id):
    global server_msg_id
    msg = struct.pack("!BH", CONFIRM, ref_id)
    server_msg_id += 1
    return msg

def build_reply(code, ref_id, message):
    global server_msg_id
    msg = struct.pack("!BHBH", REPLY, server_msg_id, code, ref_id) + message.encode("utf-8") + b"\x00"
    server_msg_id += 1
    return msg

def build_msg(display_name, message):
    global server_msg_id
    msg = struct.pack("!BH", MSG, server_msg_id)
    server_msg_id += 1
    return msg + display_name.encode("utf-8") + b"\x00" + message.encode("utf-8") + b"\x00"

def build_err(display_name, message):
    global server_msg_id
    msg = struct.pack("!BH", ERR, server_msg_id)
    server_msg_id += 1
    return msg + display_name.encode("utf-8") + b"\x00" + message.encode("utf-8") + b"\x00"

def build_bye(display_name):
    global server_msg_id
    msg = struct.pack("!BH", BYE, server_msg_id)
    server_msg_id += 1
    return msg + display_name.encode("utf-8") + b"\x00"

def build_ping():
    global server_msg_id
    msg = struct.pack("!BH", PING, server_msg_id)
    server_msg_id += 1
    return msg

def parse_string_fields(data, start_pos=0):
    fields = []
    pos = start_pos
    
    while pos < len(data):
        end = data.find(b'\x00', pos)
        if end == -1:
            break
        fields.append(data[pos:end].decode('utf-8', errors='ignore'))
        pos = end + 1
        if pos >= len(data):
            break
    
    return fields

def simulate_packet_delay():
    """Simulates a random packet delay."""
    delay = random.uniform(PACKET_DELAY_MIN, PACKET_DELAY_MAX)
    time.sleep(delay)

def initial_listener():
    """Listens on the initial port for AUTH messages"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, INITIAL_PORT))
    print(f"[SERVER] Listening for initial connections on {HOST}:{INITIAL_PORT} ...")
    
    while True:
        data, addr = sock.recvfrom(1024)
        if len(data) < 3:
            continue
            
        msg_type = data[0]
        msg_id = struct.unpack("!H", data[1:3])[0]
        
        print(f"\n[RECEIVED] Type: 0x{msg_type:02X}, ID: {msg_id}, From: {addr}")
        
        # Send confirm immediately
        if msg_type != CONFIRM:
            confirm_msg = build_confirm(msg_id)
            simulate_packet_delay()  # Simulate packet delay
            sock.sendto(confirm_msg, addr)
            print(f"[SENT] Confirm for ID {msg_id}")
        
        # Process AUTH messages
        if msg_type == AUTH:
            # Extract auth information
            try:
                fields = parse_string_fields(data, 3)
                if len(fields) >= 3:
                    username, display_name, secret = fields[:3]
                    print(f"[AUTH] Username: {username}, DisplayName: {display_name}")
                    
                    # Create dedicated socket for this client
                    client_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    client_sock.bind((HOST, 0))  # Use dynamic port
                    _, dyn_port = client_sock.getsockname()
                    
                    # Store client information
                    global client
                    client = {
                        'addr': addr,
                        'socket': client_sock,
                        'seen_ids': set([msg_id]),
                        'display_name': display_name,
                        'username': username
                    }
                    
                    # Start dedicated listener for this client
                    threading.Thread(
                        target=client_listener, 
                        args=(client_sock, addr),
                        daemon=True
                    ).start()
                    
                    # Send reply from the new dynamic port
                    reply_msg = build_reply(1, msg_id, "Authentication successful")
                    client_sock.sendto(reply_msg, addr)
                    print(f"[SENT] Auth success reply from dynamic port {dyn_port}")
                else:
                    print("[AUTH] Invalid format - missing fields")
            except Exception as e:
                print(f"[ERROR] Failed to process AUTH: {e}")
        else:
            print(f"[WARN] Received non-AUTH message on initial port: 0x{msg_type:02X}")

def client_listener(sock, client_addr):
    """Dedicated listener for the single client after authentication"""
    _, port = sock.getsockname()
    print(f"[SERVER] Started dedicated listener on port {port} for {client_addr}")
    
    global client
    while client and client['addr'] == client_addr:
        try:
            data, addr = sock.recvfrom(1024)
            if addr != client_addr:
                print(f"[WARN] Received message from unexpected address: {addr}")
                continue
                
            if len(data) < 3:
                continue
                
            msg_type = data[0]
            msg_id = struct.unpack("!H", data[1:3])[0]
            
            print(f"\n[RECEIVED] Type: 0x{msg_type:02X}, ID: {msg_id}, From: {addr}")
            
            # Send confirm immediately
            confirm_msg = build_confirm(msg_id)
            simulate_packet_delay()  # Simulate packet delay
            sock.sendto(confirm_msg, addr)
            print(f"[SENT] Confirm for ID {msg_id}")
            
            # Check if we've seen this message ID
            if msg_id in client['seen_ids']:
                print(f"[INFO] Duplicate message ID {msg_id} - already processed")
                continue
            
            # Mark message as seen
            client['seen_ids'].add(msg_id)
            
            # Process message based on type
            if msg_type == JOIN:
                fields = parse_string_fields(data, 3)
                if len(fields) >= 2:
                    channel_id, display_name = fields[:2]
                    print(f"[JOIN] Channel: {channel_id}, DisplayName: {display_name}")
                    
                    # Update client info
                    client['display_name'] = display_name
                    
                    # Inform the client of successful join
                    reply_msg = build_reply(1, msg_id, f"Joined channel {channel_id}")
                    sock.sendto(reply_msg, addr)
                    print(f"[SENT] Join success reply")
                
            elif msg_type == MSG:
                fields = parse_string_fields(data, 3)
                if len(fields) >= 2:
                    sender_name, message = fields[:2]
                    print(f"[MSG] From: {sender_name}, Message: {message}")
                    
                    # Just acknowledge the message since there's only one client
                    pass
            
            elif msg_type == PING:
                # Just send the confirmation, which we already did
                pass
                
            elif msg_type == BYE:
                fields = parse_string_fields(data, 3)
                if fields:
                    display_name = fields[0]
                    print(f"[BYE] From: {display_name}")
                  
                    client = None
                    print(f"[SERVER] Client {addr} disconnected")
                    break
            
            elif msg_type == CONFIRM or msg_type == REPLY:
                # These are responses, not requests - just log them
                if msg_type == REPLY:
                    result = struct.unpack("!H", data[3:5])[0]
                    ref_id = struct.unpack("!H", data[5:7])[0]
                    try:
                        message = data[7:].split(b'\x00')[0].decode('utf-8')
                        print(f"[REPLY] Result: {result}, Ref ID: {ref_id}, Message: {message}")
                    except:
                        print(f"[REPLY] Result: {result}, Ref ID: {ref_id}")
                else:
                    ref_id = struct.unpack("!H", data[3:5])[0]
                    print(f"[CONFIRM] Ref ID: {ref_id}")
            
            else:
                print(f"[WARN] Unsupported message type: 0x{msg_type:02X}")
                
        except Exception as e:
            print(f"[ERROR] In client listener: {e}")
    
    print(f"[SERVER] Closed listener for {client_addr}")

# Remaining functions (print_help, command_loop, main) remain unchanged...


def print_help():
    print("\nAvailable commands:")
    print("  /help")
    print("      Show this help message.")
    print("  /client")
    print("      Show current connected client.")
    print("  /confirm <msg_id>")
    print("      Send a CONFIRM message to the client.")
    print("  /reply <code> <ref_id> <message>")
    print("      Send a REPLY message to the client. Code must be 0 or 1.")
    print("  /msg <display_name> <message>")
    print("      Send a MSG message to the client.")
    print("  /err <display_name> <message>")
    print("      Send an ERR message to the client.")
    print("  /bye <display_name>")
    print("      Send a BYE message to the client.")
    print("  /ping")
    print("      Send a PING message to the client.")
    print("  /quit")
    print("      Exit the server.\n")

def command_loop():
    time.sleep(0.1)
    while True:
        try:
            cmd = input(">>> ").strip()
        except KeyboardInterrupt:
            print("\n[SERVER] Exiting.")
            break

        if cmd == "/help":
            print_help()
        
        elif cmd == "/client":
            if client:
                print(f"\nConnected client: {client['addr']} - {client['display_name']}")
            else:
                print("\nNo client connected")
            print()
        
        elif cmd.startswith("/confirm "):
            try:
                ref_id = int(cmd.split(" ", 1)[1])
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_confirm(ref_id)
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Confirm for ID {ref_id}")
            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /confirm <msg_id>")
        
        elif cmd.startswith("/reply "):
            try:
                parts = cmd.split(" ", 3)
                code = int(parts[1])
                if code not in [0, 1]:
                    print("[ERROR] Code must be 0 or 1")
                    continue
                ref_id = int(parts[2])
                message = parts[3]
                
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_reply(code, ref_id, message)
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Reply (code={code}, ref={ref_id}, msg={message})")

            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /reply <code> <ref_id> <message>")
        
        elif cmd.startswith("/msg "):
            print("[INFO] Sending message to client...")
            try:
                parts = cmd.split(" ", 2)
                display_name = parts[1]
                message = parts[2]
                
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_msg(display_name, message)
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Message from {display_name}: {message}")

            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /msg <display_name> <message>")
        
        elif cmd.startswith("/err "):
            try:
                parts = cmd.split(" ", 2)
                display_name = parts[1]
                message = parts[2]
                
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_err(display_name, message)
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Error from {display_name}: {message}")

            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /err <display_name> <message>")
        
        elif cmd.startswith("/bye "):
            try:
                display_name = cmd.split(" ", 1)[1]
                
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_bye(display_name)
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Bye from {display_name}")
            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /bye <display_name>")
        
        elif cmd == "/ping":
            try:
                if not client:
                    print("[ERROR] No client connected")
                    continue
                    
                client_sock = client['socket']
                msg = build_ping()
                client_sock.sendto(msg, client['addr'])
                print(f"[SENT] Ping")
            except Exception as e:
                print(f"[ERROR] {e}")
                print("[ERROR] Usage: /ping")
        
        elif cmd == "/quit":
            print("[SERVER] Shutting down...")
            break
            
        else:
            print("[INFO] Unknown command. Type /help for available commands.")

def main():
    # Start the initial listener on a separate thread
    threading.Thread(target=initial_listener, daemon=True).start()
    
    # Start the command loop on the main thread
    command_loop()

if __name__ == "__main__":
    main()
