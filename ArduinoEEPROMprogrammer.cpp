// EEPROMProgrammerConsole.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <ctype.h>
#include <conio.h>
#include <fstream>

#pragma comment(lib, "Setupapi.lib")

#include "SerialClass.h"

#define KEY_ENTER 10
#define KEY_UP 72
#define KEY_DOWN 80
#define KEY_LEFT 75
#define KEY_RIGHT 77

#define READ 0
#define VERIFY 1
#define DUMP 2
#define WRITE 3
#define ERASE 4
#define EXIT 5

std::vector<int> GetAvailableCOMPorts();
std::vector<std::string> GetDeviceNames();

static unsigned char* data = new unsigned char[8192];

void wait_for_key_press() {
    HANDLE hstdin;
    DWORD  mode;
    hstdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hstdin, &mode);
    SetConsoleMode(hstdin, ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

    int ch = 10;
    while (ch == 10) {
        ch = std::cin.get();
    }

    COORD tl = { 0,0 };
    CONSOLE_SCREEN_BUFFER_INFO s;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &s);
    DWORD written, cells = s.dwSize.X * s.dwSize.Y;
    FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
    FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
    SetConsoleCursorPosition(console, tl);

    SetConsoleMode(hstdin, mode);
}

namespace mainmenu {
    void read_into_buffer(Serial* SP) {
        int arr[1] = { 0xab };
        DWORD bytesSend;
        if (!WriteFile(SP->hSerial, arr, 1, &bytesSend, 0)) {
            std::cout << std::endl << "An error occured while trying to communicate with Arduino. Press any key to continue...";
            wait_for_key_press();
            return;
        }
        for (int i = 0; i < 8192; i++) {
            unsigned char c = '\0';
            DWORD bytesRead;
            if (!ReadFile(SP->hSerial, &c, 1, &bytesRead, NULL)) {
                std::cout << "An error occured while reading EEPROM. Press any key to continue... ";
                wait_for_key_press();
                return;
            }
            data[i] = c;
        }
    }
    DWORD write_to_serial(Serial* SP, std::string data) {
        DWORD bytesSent;
        if (!WriteFile(SP->hSerial, data.c_str(), data.size(), &bytesSent, 0)) {
            std::cout << "\nFATAL ERROR: Connection was interrupted during data transfer. Please restart application. Press any key to continue... ";
            wait_for_key_press();
            throw std::exception();
        }
        return bytesSent;
    }
    void print() {
        COORD tl = { 0,0 };
        CONSOLE_SCREEN_BUFFER_INFO s;
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(console, &s);
        DWORD written, cells = s.dwSize.X * s.dwSize.Y;
        FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
        FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
        SetConsoleCursorPosition(console, tl);

        for (int i = 0; i < 8192; i += 16) {
            printf("0x%04x:  ", i);
            for (int j = 0; j < 16; j++) {
                printf("%02hhx ", data[i + j]);
                if (j == 7) printf(" ");
            }
            printf(" | ");
            for (int j = 0; j < 16; j++) {
                char c = isprint(data[i + j]) ? data[i + j] : '.';
                printf("%c", c);
            }
            printf(" |\n");
        }
        std::cout << "Press any key to continue... ";
        wait_for_key_press();
    }
    void dump() {
        OPENFILENAMEA ofn;
        char szFileName[MAX_PATH];
        char szFileTitle[MAX_PATH];
        char filePath[MAX_PATH];
        char szFile[MAX_PATH] = "contents.bin";

        *szFileName = 0;
        *szFileTitle = 0;

        ofn.lpstrFile = szFile;
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = GetFocus();
        ofn.lpstrFilter = "Binary File (*.bin)\0*.bin\0ROM File (*.rom)\0*.rom\0All Files (*.*)\0*.*\0";
        ofn.lpstrCustomFilter = NULL;
        ofn.nMaxCustFilter = NULL;
        ofn.nFilterIndex = NULL;
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = "."; // Initial directory.
        ofn.lpstrFileTitle = szFileTitle;
        ofn.nMaxFileTitle = MAX_PATH;
        ofn.lpstrTitle = "Save EEPROM contents to a file...";
        ofn.lpstrDefExt = "*.bin";

        ofn.Flags = OFN_OVERWRITEPROMPT;

        if (!GetSaveFileNameA((LPOPENFILENAMEA)&ofn))
            return;
        else strcpy_s(filePath, ofn.lpstrFile);

        std::ofstream in;
        in.open(ofn.lpstrFile, std::ios::out | std::ios::binary | std::ofstream::trunc);

        in.write(reinterpret_cast<const char*>(data), 8192);

        std::cout << "EEPROM contents have been successfully dumped! Press any key to continue... ";
        wait_for_key_press();
    }
    void verify() {
        OPENFILENAMEA ofn;
        char szFileName[MAX_PATH];
        char szFileTitle[MAX_PATH];

        *szFileName = 0;
        *szFileTitle = 0;

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = GetFocus();
        ofn.lpstrFilter = "Binary File (*.bin)\0*.bin\0ROM File (*.rom)\0*.rom\0All Files (*.*)\0*.*\0";
        ofn.lpstrCustomFilter = NULL;
        ofn.nMaxCustFilter = NULL;
        ofn.nFilterIndex = NULL;
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = "."; // Initial directory.
        ofn.lpstrFileTitle = szFileTitle;
        ofn.nMaxFileTitle = MAX_PATH;
        ofn.lpstrTitle = "Open a file to verify with...";
        ofn.lpstrDefExt = "*.bin";

        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameA((LPOPENFILENAMEA)&ofn))
            return;
        std::ifstream in;
        in.open(ofn.lpstrFile, std::ios::in | std::ios::binary);

        DWORD bytesRead;
        std::vector<int> addresses;

        for (int i = 0; i < 8192 && !in.eof(); i++) {
            unsigned char ch;
            in >> ch;
            DWORD bytesSend;
            
            if (data[i] != ch) {
                addresses.push_back(i);
            }
        }
        if (addresses.empty()) {
            std::cout << "The selected file and the EEPROM contents are identical! ";
        }
        else {
            std::cout << "Data mismatch found in " << addresses.size() << " addresses:" << std::endl;
            for (int const& a : addresses) {
                printf("0x%04x, ", a);
            }
            std::cout << std::endl;
        }
        std::cout << "Press any key to continue... ";
        wait_for_key_press();
    }

    void write(Serial* SP) {
        OPENFILENAMEA ofn;
        char szFileName[MAX_PATH];
        char szFileTitle[MAX_PATH];

        *szFileName = 0;
        *szFileTitle = 0;

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = GetFocus();
        ofn.lpstrFilter = "Binary File (*.bin)\0*.bin\0ROM File (*.rom)\0*.rom\0All Files (*.*)\0*.*\0";
        ofn.lpstrCustomFilter = NULL;
        ofn.nMaxCustFilter = 0;
        ofn.nFilterIndex = 0;
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = "."; // Initial directory.
        ofn.lpstrFileTitle = szFileTitle;
        ofn.nMaxFileTitle = MAX_PATH;
        ofn.lpstrTitle = "Open a file to write to EEPROM...";
        ofn.lpstrDefExt = "*.bin";

        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (!GetOpenFileNameA((LPOPENFILENAMEA)&ofn)) {
            COORD tl = { 0,0 };
            CONSOLE_SCREEN_BUFFER_INFO s;
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            GetConsoleScreenBufferInfo(console, &s);
            DWORD written, cells = s.dwSize.X * s.dwSize.Y;
            FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
            FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
            SetConsoleCursorPosition(console, tl);
            return;
        }
        std::ifstream in;
        in.open(ofn.lpstrFile, std::ios::in | std::ios::binary);

        COORD tl = { 0,0 };
        CONSOLE_SCREEN_BUFFER_INFO s;
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(console, &s);
        DWORD written, cells = s.dwSize.X * s.dwSize.Y;
        FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
        FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
        SetConsoleCursorPosition(console, tl);

        DWORD bytesSend;
        std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        file.resize(8192);

        for (int i = 0; i < 8192; i += 16) {
            printf("0x%04x:  ", i);
            for (int j = 0; j < 16; j++) {
                printf("%02hhx ", file[i + j]);
                if (j == 7) printf(" ");
            }
            printf("\n");
        }

        std::cout << "\nYou're about to overwrite the data in the EEPROM with the data above.\n"
            << "Press Enter to proceed, Space to cancel. ";
        while (true) {
            int c = _getch();
            if (c == 32) {
                COORD tl = { 0,0 };
                CONSOLE_SCREEN_BUFFER_INFO s;
                HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
                GetConsoleScreenBufferInfo(console, &s);
                DWORD written, cells = s.dwSize.X * s.dwSize.Y;
                FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
                FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
                SetConsoleCursorPosition(console, tl);
                return;
            }
            else if (c == 13) break;
        }

        std::cout << std::endl << "Writing data...";
        DWORD bytesSent = NULL;
        write_to_serial(SP, std::string(1, static_cast<char>(0xAA)));
        for (size_t i = 0; i < 8192; i++) {
            bytesSent += write_to_serial(SP, std::string(1, file[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << std::endl << bytesSent << " bytes have been written. Reading the EEPROM into the buffer...";
        read_into_buffer(SP);
        std::cout << "\nSuccess. Press any key to continue... ";
        wait_for_key_press();
    }

    void erase(Serial* SP) {
        std::cout << "\nYou're about to erase all data in the EEPROM.\n"
            << "Press Enter to proceed, Space to cancel. ";
        while (true) {
            int c = _getch();
            if (c == 32) {
                COORD tl = { 0,0 };
                CONSOLE_SCREEN_BUFFER_INFO s;
                HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
                GetConsoleScreenBufferInfo(console, &s);
                DWORD written, cells = s.dwSize.X * s.dwSize.Y;
                FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
                FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
                SetConsoleCursorPosition(console, tl);
                return;
            }
            else if (c == 13) break;
        }

        std::cout << std::endl << "Erasing...";
        std::string data(8192, static_cast<char>(0x00));
        DWORD bytesSent = NULL;
        write_to_serial(SP, std::string(1, static_cast<char>(0xAA)));
        for (size_t i = 0; i < 8192; i++) {
            bytesSent += write_to_serial(SP, std::string(1, data[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << std::endl << bytesSent << " bytes have been written. Reading the EEPROM into the buffer...";
        read_into_buffer(SP);
        std::cout << "\nSuccess. Press any key to continue... ";
        wait_for_key_press();
    }
}

int main(int argc, char* argv[]) {
    try {
        Serial* SP;
        unsigned sel;
        std::vector<int> comPorts;
        while (true) {
            while (true) {
                comPorts = GetAvailableCOMPorts();
                if (comPorts.size() == 0) {
                    std::cout << "No Arduino detected! Ensure proper connectivity, then press any key to retry... ";
                    wait_for_key_press();
                    continue;
                }
                std::vector<std::string> deviceNames = GetDeviceNames();
                sel = 0;
                if (comPorts.size() >= 2) {
                    std::cout << "Select serial port..." << std::endl;
                    for (int i = 0; i < comPorts.size(); i++) {
                        std::cout << (!i ? "> " : "  ") << deviceNames[i] << std::endl;
                    }
                    unsigned int ch = 0;
                    bool loop = true;
                    while (loop) {
                        ch = 0;
                        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
                        switch (ch = _getch()) {
                        case KEY_DOWN:
                            SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                            std::cout << (char)(0x20) << std::flush;
                            ((sel != comPorts.size() - 1) ? sel++ : sel = 0);
                            SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                            std::cout << ">" << std::flush;
                            SetConsoleCursorPosition(output, { 0, (short)(comPorts.size() + 1) });
                            break;
                        case KEY_UP:
                            SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                            std::cout << (char)(0x20) << std::flush;
                            ((sel != 0) ? sel-- : sel = static_cast<unsigned int>(comPorts.size()) - 1);
                            SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                            std::cout << ">" << std::flush;
                            SetConsoleCursorPosition(output, { 0, (short)(comPorts.size() + 1) });
                            break;
                        case VK_RETURN:
                            loop = false;
                            break;
                        }
                    }
                }
                char def[11] = "\\\\.\\COM";
                char port[15]{};
                sprintf_s(port, "%s%u", def, comPorts[sel]);
                std::cout << "Connecting to " << std::string("COM") + std::to_string(comPorts[sel]) << "...";
                SP = new Serial(port);

                if (!SP->IsConnected()) {
                    std::cout << std::endl << "Connection failed! Close all other instances and try again. Press any key to continue... ";
                    wait_for_key_press();
                    continue;
                }
                break;
            }
            std::cout << std::endl << "Connection established! Baud rate: 57600\nWaiting for data...";
            mainmenu::read_into_buffer(SP);

            COORD tl = { 0,0 };
            CONSOLE_SCREEN_BUFFER_INFO s;
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            GetConsoleScreenBufferInfo(console, &s);
            DWORD written, cells = s.dwSize.X * s.dwSize.Y;
            FillConsoleOutputCharacter(console, ' ', cells, tl, &written);
            FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
            SetConsoleCursorPosition(console, tl);

            while (true) {
                if (!SP->IsConnected()) {
                    std::cout << "Connection lost to " << std::string("COM") + std::to_string(comPorts[sel]) << ". Press any key to reconnect... ";
                    wait_for_key_press();
                    break;
                }
                std::cout << "Select an operation to continue...\n"
                    "> Read\n"
                    "  Verify\n"
                    "  Dump to file\n"
                    "  Write\n"
                    "  Erase\n"
                    "  Exit\n";
                unsigned int ch = 0, sel = 0;
                bool loop = true;
                while (loop) {
                    ch = 0;
                    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
                    switch (ch = _getch()) {
                    case KEY_DOWN:
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << (char)(0x20) << std::flush;
                        ((sel != 5) ? sel++ : sel = 0);
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << ">" << std::flush;
                        SetConsoleCursorPosition(output, { 0, 7 });
                        break;
                    case KEY_UP:
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << (char)(0x20) << std::flush;
                        ((sel != 0) ? sel-- : sel = 5);
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << ">" << std::flush;
                        SetConsoleCursorPosition(output, { 0, 7 });
                        break;
                    case VK_RETURN:
                        loop = false;
                        break;
                    }
                }
                switch (sel) {
                case READ:
                    mainmenu::print();
                    break;
                case VERIFY:
                    mainmenu::verify();
                    break;
                case DUMP:
                    mainmenu::dump();
                    break;
                case WRITE:
                    mainmenu::write(SP);
                    break;
                case ERASE:
                    mainmenu::erase(SP);
                    break;
                case EXIT:
                    return 0;
                }
            }
        }
        return 0;
    }
    catch (const std::exception&) {
        return 1;
    }
}

std::vector<std::string> GetDeviceNames() {
    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);

    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, nullptr, DIGCF_PRESENT);
    if (hDeviceInfo == INVALID_HANDLE_VALUE) {
        throw std::exception();
    }

    int nDevice = 0;
    std::vector<std::string> names;
    while (SetupDiEnumDeviceInfo(hDeviceInfo, nDevice++, &devInfoData)) {
        DWORD regDataType;
        DWORD reqSize = 0;

        SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &devInfoData, SPDRP_HARDWAREID, nullptr, nullptr, 0, &reqSize);
        BYTE* hardwareId = new BYTE[MAX_PATH];
        if (SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &devInfoData, SPDRP_HARDWAREID, &regDataType, hardwareId, sizeof(hardwareId) * reqSize, nullptr)) {
            reqSize = 0;
            SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, nullptr, nullptr, 0, &reqSize);
            BYTE* friendlyName = new BYTE[MAX_PATH];

            if (!SetupDiGetDeviceRegistryPropertyW(hDeviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, nullptr, friendlyName, sizeof(friendlyName) * reqSize, nullptr)) {
                memset(friendlyName, 0, reqSize > 1 ? reqSize : 1);
            }

            std::string deviceName;
            for (int i = 0; i < MAX_PATH; i++) {
                char c = *(friendlyName + i);
                deviceName += c;
                if (c == ')') break;
            }
            names.push_back(deviceName);

            delete[] friendlyName;
        }
        delete[] hardwareId;
    }
    return names;
}

std::vector<int> GetAvailableCOMPorts() {
    char lpTargetPath[5000];
    std::vector<int> output;

    for (int i = 0; i < 255; i++) {
        std::string str = "COM" + std::to_string(i);
        DWORD test = QueryDosDeviceA(str.c_str(), lpTargetPath, 5000);

        if (test) {
            output.push_back(i);
        }
    }
    return output;
}