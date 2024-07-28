#include "stdafx.h"
#include "afxsock.h"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

#define BUFFER_SIZE 1024

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;
bool running = true;

using namespace std;

void ShowCur(bool CursorVisibility)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursor = { 1, CursorVisibility };
    SetConsoleCursorInfo(handle, &cursor);
}

void signalHandler(int signum) {
    cout << "Interrupt signal (" << signum << ") received. Shutting down client...\n";
    running = false;
}

// Hàm tải danh sách các file đã được download
unordered_set<string> loadDownloadedFiles(const char* fileName) {
    ifstream file(fileName);
    unordered_set<string> downloadedFiles;
    string line;

    while (getline(file, line)) {
        downloadedFiles.insert(line);
    }

    file.close();
    return downloadedFiles;
}

// Hàm lưu tên file đã được download
void saveDownloadedFile(const char* fileName, const string& downloadedFile) {
    ofstream file(fileName, ios::app);
    if (file.is_open()) {
        file << downloadedFile << endl;
        file.close();
    }
    else {
        cerr << "Error opening file: " << fileName << endl;
    }
}

// Hàm lấy danh sách các file mới cần download từ input.txt
vector<string> getNewFilesToDownload(const char* inputFileName, const unordered_set<string>& downloadedFiles) {
    ifstream file(inputFileName);
    vector<string> newFiles;
    string line;

    while (getline(file, line)) {
        if (downloadedFiles.find(line) == downloadedFiles.end()) {
            newFiles.push_back(line);
        }
    }

    file.close();
    return newFiles;
}

// Hàm download file từ server
bool downloadFile(CSocket& serverSocket, const string& fileName) {
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    strcpy_s(buffer, fileName.c_str());
    serverSocket.Send(buffer, fileName.size());

    ofstream outFile("output/" + fileName, ios::binary);
    if (!outFile.is_open()) {
        cerr << "Error creating file to save the download" << endl;
        return false;
    }

    int bytesReceived;
    long long totalBytesReceived = 0;
    long long filesize;
    serverSocket.Receive(&filesize, sizeof(filesize));

    while ((bytesReceived = serverSocket.Receive(buffer, BUFFER_SIZE)) > 0) 
    /*while (totalBytesReceived < filesize)*/{
        if (string(buffer, bytesReceived) == "ERROR") {
            cerr << "Error: File not found on server" << endl;
            outFile.close();
            remove(("output/" + fileName).c_str());
            return false;
        }
        outFile.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;
        cout << "\rDownloading " << fileName << " .... " << (totalBytesReceived * 100) / filesize << " %";
        memset(buffer, 0, BUFFER_SIZE); // Làm sạch buffer sau khi nhận dữ liệu
        if (totalBytesReceived == filesize) {
            break;
        }
    }
    cout << endl;

    outFile.close();
    cout << "File downloaded successfully: " << fileName << endl;
    saveDownloadedFile("downloaded_files.txt", fileName);  // Ghi tên file đã download vào downloaded_files.txt

    return true;
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[]) {
    ShowCur(false);
    signal(SIGINT, signalHandler);
    int nRetCode = 0;
    if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) {
        _tprintf(_T("Fatal Error: MFC initialization failed\n"));
        nRetCode = 1;
    }
    else {
        if (AfxSocketInit() == false) {
            cout << "Khong the khoi tao Socket Library " << endl;
            return 1;
        }

        CSocket clientSocket;
        if (!clientSocket.Create()) {
            cout << "Khong the tao socket " << endl;
            return 1;
        }

        if (clientSocket.Connect(_T("192.168.31.215"), 1234) != 0) {
            cout << "Ket noi toi server thanh cong !!" << endl << endl;

            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            clientSocket.Receive(buffer, BUFFER_SIZE);

            cout << "Available files from server: \n" << buffer << endl;


            unordered_set<string> downloadedFiles = loadDownloadedFiles("downloaded_files.txt");

            while (running) {
                // Lấy danh sách các file cần download
                vector<string> newFilesToDownload = getNewFilesToDownload("input.txt", downloadedFiles);

                // Download từng file mới
                for (const auto& filename : newFilesToDownload) {
                    if (downloadFile(clientSocket, filename)) {
                        downloadedFiles.insert(filename);  // Cập nhật danh sách các file đã download
                    }
                    else {
                        cerr << "Failed to download file: " << filename << endl;
                    }
                }

                // Kiểm tra xem còn file nào mới cần download không
                newFilesToDownload = getNewFilesToDownload("input.txt", downloadedFiles);
                if (newFilesToDownload.empty()) {
                    cout << "No new files to download. Waiting for new entries in input.txt..." << endl;
                    this_thread::sleep_for(chrono::seconds(2)); // Thêm thời gian chờ ngắn trước khi quét lại
                }
            }
        }
        else {
            cout << "Khong the ket noi duoc voi server " << endl;
        }

        clientSocket.Close();
        cout << "Client da dong !" << endl;
    }
    return nRetCode;
}