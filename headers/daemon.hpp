#ifndef DAEMON_HPP
#define DAEMON_HPP

#include <algorithm> 
#include <cctype>
#include <locale>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include "./tintin_reporter.hpp"
#include <fstream>

class daemon_class
{
public:
    static volatile sig_atomic_t daemon_running;  // Flag to control the daemon's main loop

    // Signal handler function
    static void signal_handler(int signal) {
        switch(signal) {
            // Signals that stop the program
            case SIGHUP: 
            case SIGINT:  
            case SIGQUIT: 
            case SIGTERM:
                logger.log(0 ,"Signal received: " + std::to_string(signal) + ", stopping daemon...");
                daemon_running = 0; // Stop the daemon
                break;
            
            // Signals that are ignored or handled differently
            case SIGCHLD:
            case SIGPWR:
            case SIGURG:
                logger.log(0,"Signal received: " + std::to_string(signal));
                break;

            default:
                logger.log(0,"Signal received: " + std::to_string(signal));
                break;
        }
    }
    // Constructor
    daemon_class()
    {
        // if (fileExists("/var/lock/matt_daemon.lock"))
        // {
        //     throw std::runtime_error("Error: Could not create lock file /var/run/matt_daemon.lock. Daemon is likely already running.");
        //     logger.log(-1,"Error file locked.");
        //     exit(EXIT_FAILURE);
        // }

        logger.log(0, "matt_daemon starts");

        pid = fork();
        if (pid > 0)
            exit(EXIT_SUCCESS);
        else if (pid < 0)
            exit(EXIT_FAILURE);

        umask(0);

        // openlog("matt_daemon", LOG_NOWAIT | LOG_PID, LOG_USER);
        logger.log(0,"Successfully started matt_daemon");

        sid = setsid();
        if (sid < 0)
        {
            logger.log(-1, "Could not generate session ID for child process");
            exit(EXIT_FAILURE);
        }

        if ((chdir("/")) < 0)
        {
            logger.log(-1, "Could not change working directory to /");
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        createFile("/var/lock/matt_daemon.lock");
        logger.log(0, "Daemon started successfully");
        std::cout <<  "Matt_daemon started successfully" << std::endl;
        // Register signal handlers
        signal(SIGHUP, signal_handler);   // Terminal hangup
        signal(SIGINT, signal_handler);   // Interrupt from keyboard (Ctrl + C)
        signal(SIGQUIT, signal_handler);  // Quit from keyboard (Ctrl + \)
        signal(SIGTERM, signal_handler);  // Termination signal
        signal(SIGCHLD, signal_handler);  // Child process terminated
        signal(SIGPWR, signal_handler);   // Power failure/restart
        signal(SIGURG, signal_handler);   // Urgent condition on socket


        // Start the socket listener in a separate thread
        std::thread socket_thread(&daemon_class::start_socket_listener, this);
        socket_thread.detach();

        const int SLEEP_INTERVAL = 5;

        // Main loop controlled by daemon_running flag
        while (daemon_running)
        {
            do_heartbeat();
            sleep(SLEEP_INTERVAL);
        }

        // Cleanup on exit
        logger.log(0, "Stopping matt_daemon");
        deleteFile("/var/lock/matt_daemon.lock");
        closelog();
        exit(EXIT_SUCCESS);
    }

    // Destructor
    ~daemon_class()
    {
        logger.log(0, "Daemon shutting down");
        deleteFile("/var/lock/matt_daemon.lock");
        closelog();
    }

    // Heartbeat function
    void do_heartbeat()
    {
        logger.log(0, "Daemon is alive");
        sleep(10);
    }

    // File creation utility
    bool createFile(const std::string &filePath)
    {
        std::ofstream file(filePath);
        if (file.is_open())
        {
            file.close();
            return true;
        }
        else
        {
            logger.log(-1,std::string("Failed to create file: " + filePath));
            return false;
        }
    }

    // File existence check
    bool fileExists(const std::string &filePath)
    {
        std::ifstream file(filePath);
        return file.good();
    }

    // File deletion utility
    bool deleteFile(const std::string &filePath)
    {
        try
        {
            if (std::filesystem::remove(filePath))
            {
                logger.log(0, "File deleted successfully!");
                return true;
            }
            else
            {
                logger.log(-1,"File not found.");
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            std::cerr << "Error deleting file: " << e.what() << std::endl;
            return false;
        }
    }

    // Trim from the start (left) of a string
    static inline std::string ltrim(std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        return s;
    }

    // Trim from the end (right) of a string
    static inline std::string rtrim(std::string s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
        return s;
    }

    // Trim from both ends
    static inline std::string trim(std::string s) {
        return ltrim(rtrim(s));
    }

    void start_socket_listener()
    {
        int server_fd;
        struct sockaddr_in address;
        int opt = 1;
        int active_connections = 0;  // Counter for the number of active clients
        const int MAX_CLIENTS = 3;   // Limit to 3 clients

        // Creating socket file descriptor
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            logger.log(-1, "Socket creation failed: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 4242
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        {
            logger.log(-1, "setsockopt failed: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(4242);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            logger.log(-1, "Bind failed: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, MAX_CLIENTS) < 0)
        {
            logger.log(-1, "Listen failed: " + std::string(strerror(errno)));
            exit(EXIT_FAILURE);
        }

        logger.log(0, "Socket listener started on port 4242");

        while (daemon_running)
        {
            int new_socket;
            int addrlen = sizeof(address);

            // Accept a new connection
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                logger.log(-1, "Accept failed: " + std::string(strerror(errno)));
                if (!daemon_running) {
                    break;
                }
                continue;
            }

            // Check if we have reached the maximum number of clients
            if (active_connections >= MAX_CLIENTS)
            {
                logger.log(0, "Maximum number of clients reached. New connection will be refused.");
                std::string refuse_message = "Connection refused\n";
                send(new_socket, refuse_message.c_str(), refuse_message.length(), 0);
                sleep(10);
                close(new_socket);  // Immediately close the new socket
                continue;  // Skip further processing for this connection
            }

            active_connections++;  // Increment active client count
            logger.log(0, "New client connected. Active connections: " + std::to_string(active_connections));

            // Send "Connection established" message to the client
            std::string welcome_message = "Connection established\n";
            send(new_socket, welcome_message.c_str(), welcome_message.length(), 0);

            // Start a separate thread to handle the client
            std::thread([new_socket, &active_connections, this]() {
                char buffer[1024] = {0};

                // Keep the connection open and keep reading until client disconnects or sends "quit"
                while (true)
                {
                    ssize_t valread = read(new_socket, buffer, 1024);
                    if (valread <= 0)
                    {
                        // If read() returns 0 or negative value, the client has closed the connection
                        logger.log(0, "Client disconnected.");
                        break;
                    }

                    buffer[valread] = '\0';  // Null-terminate the received message
                    std::string message = trim(std::string(buffer));  // Trim whitespace from the message
                    logger.log(1, "Received message: " + message);

                    // If the client sends "quit", terminate the daemon
                    if (message == "quit")
                    {
                        logger.log(0, "Received quit command. Shutting down daemon.");
                        daemon_running = 0;
                        break;
                    }
                }

                close(new_socket);  // Close the client socket
                active_connections--;  // Decrement active client count
                logger.log(0, "Client disconnected. Active connections: " + std::to_string(active_connections));

            }).detach();  // Detach the thread so it runs independently
        }

        // Close the server socket when daemon stops
        close(server_fd);
    }
private:
    static Tintin_reporter logger;
    pid_t pid;
    pid_t sid;
};

// Initialize static members
Tintin_reporter daemon_class::logger("/var/log/matt_daemon.log");
volatile sig_atomic_t daemon_class::daemon_running = 1;  // Initialize the running flag

#endif