#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <fstream>
#include <iostream>
#include <ctime>
#include <string>
#include <fstream>


class Tintin_reporter 
{
    public:
        //==================================================//
        Tintin_reporter(const std::string &logFilePath) : logFilePath("/var/log/matt_daemon/matt_daemon.log"), logFile(logFilePath, std::ios_base::app) 
        {
            if (fileExists("/var/lock/matt_daemon.lock"))
            {
                log(-1,"Error file locked.");
                throw std::runtime_error("Error: Could not create lock file /var/run/matt_daemon.lock. Daemon is likely already running.");
                exit(EXIT_FAILURE);
            }
            std::ifstream file ;
            file.open(logFilePath);
            file.close();
            if (createFile(logFilePath) == false) 
            {
                std::cerr << "Failed to open log file" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        Tintin_reporter(const Tintin_reporter &other) : logFilePath(other.logFilePath), logFile(logFilePath, std::ios_base::app)
        {
            if (createFile(logFilePath) == false || !logFile.is_open()) 
            {
                std::cerr << "Failed to open log file in copy constructor" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        Tintin_reporter &operator=(const Tintin_reporter &other) 
        {
            if (this != &other) 
            {
                if (logFile.is_open()) 
                    logFile.close();  

                logFilePath = other.logFilePath;
                logFile.open(logFilePath, std::ios_base::app);

                if (!logFile.is_open()) 
                {
                    std::cerr << "Failed to open log file in assignment operator" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            return *this;
        }

        ~Tintin_reporter() {
            if (logFile.is_open())
                logFile.close();
        }
        //==================================================//
        // -1 ERROR 0 INFO 1 LOG
        void log(int type, const std::string &message) 
        {
            if (logFile.is_open())
            {
                if (type == -1) 
                    logFile << getCurrentDateTime() << " [ ERROR ] - Matt_daemon: " << message << std::endl;
                else if (type == 0)
                    logFile << getCurrentDateTime() << " [ INFO ] - Matt_daemon: " << message << std::endl;
                else if (type == 1)
                    logFile << getCurrentDateTime() << " [ LOG ] - Matt_daemon: " << message << std::endl;
            }

        }
        std::string getCurrentDateTime() 
        {
            std::time_t now = std::time(nullptr);
            std::tm *localTime = std::localtime(&now);
            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "[%d / %m / %Y - %H : %M : %S]", localTime);
            return std::string(buffer);
        }
        bool createFile(const std::string &filePath) 
        {
            std::filesystem::create_directory("/var/log/matt_daemon");
            std::ofstream file(filePath);

            if (!file) {
                std::cerr << "Error: Could not create file " << filePath << std::endl;
                return false;
            }
            file.close();

            // std::cout << "File " << filePath << " created successfully." << std::endl;
            return true;
        }

        // Function to change the log file path and write to an existing file
        void setLogFilePath(const std::string &newFilePath) 
        {
            if (logFile.is_open())
                logFile.close(); // Close the currently open file

            logFilePath = newFilePath;
            logFile.open(logFilePath, std::ios_base::app); // Open the new file in append mode

            if (!logFile.is_open()) 
            {
                std::cerr << "Failed to open the log file at: " << newFilePath << std::endl;
                exit(EXIT_FAILURE);
            }
            else
                std::cout << "Log file path changed to: " << newFilePath << std::endl;
        }
        // File existence check
        bool fileExists(const std::string &filePath)
        {
            std::ifstream file(filePath);
            return file.good();
        }
    private:
        std::string logFilePath;  // Path to the log file
        std::ofstream logFile;    // File stream for logging
};

#endif

// Every daemon action should show in a log file matt_daemon.log with timestamp
// (in this form [ DD / MM / YYYY - HH : MM : SS]) located in the folder /var/log/-
// matt_daemon/.