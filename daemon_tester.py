import os
import socket
import subprocess
import time
import signal
import pwd

DAEMON_EXECUTABLE = "./Matt_daemon"
LOG_FILE = "/var/log/matt_daemon.log"
LOCK_FILE = "/var/lock/matt_daemon.lock"
DAEMON_PORT = 4242

# Helper function to check if the program is running as root
def is_running_as_root():
    return os.geteuid() == 0

# Helper function to check if a file exists
def file_exists(filepath):
    return os.path.exists(filepath)

# Helper function to read the log file
def read_log():
    if file_exists(LOG_FILE):
        with open(LOG_FILE, 'r') as log:
            return log.read()
    return ""

# Helper function to check log messages for a specific content
def log_contains(content):
    log_data = read_log()
    return content in log_data

# Helper function to count the number of clients connected to the daemon
def connect_to_socket(port, message):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect(("127.0.0.1", port))
            s.sendall(message.encode())
            return s.recv(1024).decode()
    except socket.error as e:
        return str(e)

# Start the daemon and check if it runs in the background
def start_daemon():
    if not is_running_as_root():
        print("Test failed: The program must run with root rights.")
        return False
    try:
        # Start the daemon in the background
        subprocess.Popen([DAEMON_EXECUTABLE])
        time.sleep(3)  # Give time for the daemon to initialize
        if not file_exists(LOCK_FILE):
            print("Test failed: Lock file was not created.")
            return False
        return True
    except Exception as e:
        print(f"Test failed: Unable to start daemon. Error: {e}")
        return False

# Test whether the daemon refuses a second instance
def test_single_instance():
    if file_exists(LOCK_FILE):
        second_process = subprocess.run([DAEMON_EXECUTABLE], capture_output=True, text=True)
        if "Error: Could not create lock file" not in second_process.stderr:
            print("Test failed: Second instance of daemon did not fail.")
            return False
        print("Test passed: Second instance correctly failed.")
        return True
    else:
        print("Test failed: First instance did not create lock file.")
        return False

# Test daemon's ability to receive and log messages
def test_logging_and_quit():
    # Send a regular message
    response = connect_to_socket(DAEMON_PORT, "hello")
    if "Connection refused" in response:
        print("Test failed: Connection to daemon refused.")
        return False

    # Check if the message was logged
    time.sleep(1)  # Give time for the log to update
    if not log_contains("hello"):
        print("Test failed: 'hello' message was not logged.")
        return False

    # Send "quit" message and check if daemon shuts down
    response = connect_to_socket(DAEMON_PORT, "quit")
    time.sleep(30)  # Give time for the daemon to shut down

    if not log_contains("Received quit command"):
        print("Test failed: Daemon did not log 'quit' message.")
        return False

    if file_exists(LOCK_FILE):
        print("Test failed: Lock file not removed after shutdown.")
        return False

    print("Test passed: Daemon handled messages and quit correctly.")
    return True

# Test maximum client connections
def test_max_clients():
    sockets = []
    try:
        for _ in range(3):
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(("127.0.0.1", DAEMON_PORT))
            sockets.append(s)
        
        # Fourth connection should fail
        response = connect_to_socket(DAEMON_PORT, "test")
        if "Connection refused" not in response:
            print("Test failed: Fourth connection was not refused.")
            return False

        print("Test passed: Maximum client connection limit enforced.")
        return True
    finally:
        # Close any open sockets
        for s in sockets:
            s.close()

# Test daemon's signal handling
def test_signal_handling():
    pid = subprocess.check_output(["pgrep", "-f", DAEMON_EXECUTABLE]).decode().strip()
    os.kill(int(pid), signal.SIGTERM)
    time.sleep(2)  # Give time for the log to update

    if not log_contains("Signal received: 15"):  # SIGTERM signal
        print("Test failed: SIGTERM was not logged.")
        return False

    if file_exists(LOCK_FILE):
        print("Test failed: Lock file not removed after signal termination.")
        return False

    print("Test passed: Daemon handled SIGTERM correctly.")
    return True

# Run all tests
def run_tests():
    # Test 1: Check if daemon runs with root rights and runs in the background
    if not start_daemon():
        return

    # Test 2: Ensure only one instance of daemon is running
    if not test_single_instance():
        return

    # Test 3: Check logging and quit functionality
    if not test_logging_and_quit():
        return

    # Restart the daemon to continue further testing
    if not start_daemon():
        return

    # Test 4: Check maximum client connections
    if not test_max_clients():
        return

    # Restart the daemon to continue further testing
    if not start_daemon():
        return

    # Test 5: Check signal handling
    if not test_signal_handling():
        return

    print("All tests passed successfully.")

if __name__ == "__main__":
    run_tests()
