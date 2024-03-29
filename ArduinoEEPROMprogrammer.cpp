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
#include <algorithm>

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
#define SDP 5
#define EXIT 6

#define AT28C64B 0
#define AT28C256 1

std::vector<int> GetAvailableCOMPorts();
std::vector<std::string> GetDeviceNames();

unsigned int size = 0;

static unsigned char* data = new unsigned char[32768];

void clear_screen() {
    COORD tl = { 0, 0 };
    CONSOLE_SCREEN_BUFFER_INFO s;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &s);
    DWORD written, cells = s.dwSize.X * 35000;
    FillConsoleOutputCharacterW(console, ' ', cells, tl, &written);
    FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
    SetConsoleCursorPosition(console, tl);
}

void wait_for_key_press() {
    HANDLE hstdin;
    DWORD  mode;
    hstdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hstdin, &mode);
    SetConsoleMode(hstdin, ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

    int ch = 10;
    while (ch == 10) ch = std::cin.get();

    clear_screen();
    SetConsoleMode(hstdin, mode);
}

namespace mainmenu {
    static void read_into_buffer(Serial* SP) {
        int arr[1] = { 0x01 };
        DWORD bytesSend;
        if (!WriteFile(SP->hSerial, arr, 1, &bytesSend, 0)) {
            std::cout << std::endl << "An error occured while trying to communicate with Arduino. Press any key to continue...";
            wait_for_key_press();
            return;
        }
        HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO cbsi;
        GetConsoleScreenBufferInfo(hConsoleOutput, &cbsi);
        for (int i = 0; i < size; i++) {
            unsigned char c = '\0';
            DWORD bytesRead;
            if (!ReadFile(SP->hSerial, &c, 1, &bytesRead, NULL)) {
                std::cout << "An error occured while reading EEPROM. Press any key to continue... ";
                wait_for_key_press();
                return;
            }
            data[i] = c;
            std::cout << static_cast<int>(i * (100.f / size)) << "%";
            SetConsoleCursorPosition(hConsoleOutput, cbsi.dwCursorPosition);
        }
        std::cout << "100%";
    }
    static DWORD write_to_serial(Serial* SP, std::string data) {
        DWORD bytesSent;
        if (!WriteFile(SP->hSerial, data.c_str(), data.size(), &bytesSent, 0)) {
            std::cout << "\nFATAL ERROR: Connection was interrupted during data transfer. Please restart application. Press any key to continue... ";
            wait_for_key_press();
            throw std::exception();
        }
        return bytesSent;
    }
    static void print() {
        clear_screen();

        for (int i = 0; i < size; i += 16) {
            std::cout << std::hex << std::setfill('0') << "0x" << std::setw(4) << i << ":  ";
            for (int j = 0; j < 16; j++) {
                std::cout << std::hex << std::setw(2) << static_cast<int>(data[i + j]) << ' ';
                if (j == 7) std::cout << ' ';
            }
            std::cout << " | ";
            for (int j = 0; j < 16; j++) {
                char c = std::isprint(data[i + j]) ? static_cast<char>(data[i + j]) : '.';
                std::cout << c;
            }
            std::cout << " |\n";
        }
        std::cout << std::dec << "Press any key to continue... ";
        wait_for_key_press();
    }
    static void dump() {
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

        in.write(reinterpret_cast<const char*>(data), size);

        std::cout << "EEPROM contents have been successfully dumped! Press any key to continue... ";
        wait_for_key_press();
    }
    static void verify() {
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
        std::vector<int> addrs;

        std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        int length = file.size();
        file.resize(size);
        for (int i = length; i < size; i++) {
            file[i] = 0x00;
        }
        for (int i = 0; i < size; i++) {
            if (data[i] != static_cast<unsigned char>(file[i])) {
                addrs.push_back(i);
            }
        }

        if (addrs.empty()) {
            std::cout << "The selected file and the EEPROM contents are identical! ";
        }
        else {
            std::cout << "Data mismatch found in " << addrs.size() << " addresses:" << std::endl;
            for (int const& a : addrs) {
                printf("0x%04x, ", a);
            }
            std::cout << std::endl;
        }
        std::cout << "Press any key to continue... ";
        wait_for_key_press();
    }

    static void write(Serial* SP) {
        std::vector<int> addrs;
        std::vector<int> values;

        while (true) {
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
                clear_screen();
                return;
            }
            std::ifstream in;
            in.open(ofn.lpstrFile, std::ios::in | std::ios::binary);

            clear_screen();

            DWORD bytesSend;
            std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            int length = file.size();
            file.resize(size);
            for (int i = length; i < size; i++) {
                file[i] = 0x00;
            }

            addrs.erase(addrs.begin(), addrs.end());
            values.erase(values.begin(), values.end());
            for (int i = 0; i < size; i++) {
                if (data[i] != static_cast<unsigned char>(file[i])) {
                    addrs.push_back(i);
                    values.push_back(file[i]);
                }
            }

            if (!addrs.size()) {
                std::cout << "The contents of the file you have chosen is identical to the EEPROM contents.\n" << "Press Enter to choose another file, Space to cancel. ";
                while (true) {
                    int c = _getch();
                    if (c == 32) {
                        clear_screen();
                        return;
                    }
                    else if (c == 13) {
                        break;
                    }
                }
            }
            else {
                for (int i = 0; i < size; i += 16) {
                    printf("0x%04x:  ", i);
                    for (int j = 0; j < 16; j++) {
                        printf("%02hhx ", file[i + j]);
                        if (j == 7) printf(" ");
                    }
                    printf(" | ");
                    for (int j = 0; j < 16; j++) {
                        char c = isprint((unsigned char)file[i + j]) ? (unsigned char)file[i + j] : '.';
                        printf("%c", c);
                    }
                    printf(" |\n");
                }
                break;
            }
        }

        std::cout << "\nYou're about to overwrite the data in the EEPROM with the data above.\n"
            << "Press Enter to proceed, Space to cancel. ";
        while (true) {
            int c = _getch();
            if (c == 32) {
                clear_screen();
                return;
            }
            else if (c == 13) break;
        }

        clear_screen();
        
        write_to_serial(SP, std::string(1, static_cast<char>(0x00)));
        std::cout << "Writing data... ";
        write_to_serial(SP, std::string(1, (unsigned char)(values.size() >> 8)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size())));
        DWORD bytesSent = NULL;
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        for (size_t i = 0; i < values.size(); i++) {
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i] >> 8)));
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i])));
            bytesSent += write_to_serial(SP, std::string(1, values[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::cout << static_cast<int>(i / (size / 100)) << "%";
            SetConsoleCursorPosition(output, { 16, 0 });
        }

        std::cout << std::dec << std::endl << bytesSent << " bytes have been written. Reading the EEPROM into the buffer... ";
        read_into_buffer(SP);
        std::cout << "\nSuccess. Press any key to continue... ";
        wait_for_key_press();
    }

    static void erase(Serial* SP) {
        std::cout << "\nYou're about to erase all data in the EEPROM.\n"
            << "Press Enter to proceed, Space to cancel. ";
        while (true) {
            int c = _getch();
            if (c == 32) {
                clear_screen();
                return;
            }
            else if (c == 13) break;
        }

        std::cout << std::endl << "Erasing... ";
        std::vector addrs { 0x5555, 0x1AAA, 0x5555, 0x5555, 0x1AAA, 0x5555 };
        std::vector values{ 0xAA,   0x55,   0x80,   0xAA,   0x55,   0x10   };

        write_to_serial(SP, std::string(1, static_cast<char>(0x00)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size() >> 8)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size())));
        DWORD bytesSent = NULL;
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        for (size_t i = 0; i < values.size(); i++) {
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i] >> 8)));
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i])));
            bytesSent += write_to_serial(SP, std::string(1, values[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        std::cout << "\nAll bytes have been set to 0x00. Reading the EEPROM into the buffer... ";
        read_into_buffer(SP);
        std::cout << "\nSuccess. Press any key to continue... ";
        wait_for_key_press();
    }

    static void sdt_disable(Serial* SP) {
        std::vector addrs { 0x5555, 0x2AAA, 0x5555, 0x5555, 0x2AAA, 0x5555, 0x0000, 0x0000 };
        std::vector values{ 0xAA,   0x55,   0x80,   0xAA,   0x55,   0x20,   0xEA,   0xEA   };

        write_to_serial(SP, std::string(1, static_cast<char>(0x00)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size() >> 8)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size())));
        DWORD bytesSent = NULL;
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        for (size_t i = 0; i < values.size(); i++) {
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i] >> 8)));
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i])));
            bytesSent += write_to_serial(SP, std::string(1, values[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "\nSDP (Software Data Protection) has been disabled. Press any key to continue... ";
        wait_for_key_press();
    }

    static void sdt_enable(Serial* SP) {
        std::vector addrs { 0x5555, 0x2AAA, 0x5555, 0x0000, 0x0000 };
        std::vector values{ 0xAA,   0x55,   0xA0,   0xEA,   0xEA };

        write_to_serial(SP, std::string(1, static_cast<char>(0x00)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size() >> 8)));
        write_to_serial(SP, std::string(1, (unsigned char)(values.size())));
        DWORD bytesSent = NULL;
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        for (size_t i = 0; i < values.size(); i++) {
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i] >> 8)));
            write_to_serial(SP, std::string(1, (unsigned char)(addrs[i])));
            bytesSent += write_to_serial(SP, std::string(1, values[i]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "\nSDP (Software Data Protection) has been enabled. Press any key to continue... ";
        wait_for_key_press();
    }

    static void set_sdp(Serial* SP) {
        clear_screen();
        int sel = 0;
        std::cout << "Select operation:\n"
            "> Enable SDP (Software Data Protection)\n"
            "  Disable SDP\n"
            "  Cancel\n";
        unsigned int ch = 0;
        bool loop = true;
        while (loop) {
            ch = 0;
            HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
            switch (ch = _getch()) {
            case KEY_DOWN:
                SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                std::cout << (char)(0x20) << std::flush;
                ((sel != 2) ? sel++ : sel = 0);
                SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                std::cout << ">" << std::flush;
                SetConsoleCursorPosition(output, { 0, 4 });
                break;
            case KEY_UP:
                SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                std::cout << (char)(0x20) << std::flush;
                ((sel != 0) ? sel-- : sel = 2);
                SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                std::cout << ">" << std::flush;
                SetConsoleCursorPosition(output, { 0, 4 });
                break;
            case VK_RETURN:
                loop = false;
                break;
            }
        }
        switch (sel) {
        case 0:
            mainmenu::sdt_enable(SP);
            break;
        case 1:
            mainmenu::sdt_disable(SP);
            break;
        case 2:
            return;
        }
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
            clear_screen();
            std::cout << "Connection established! Baud rate: 115200";
            int chip = 0;
            std::cout << "\nSelect the chip size:\n"
                "> 64K  (8K x 8)\n"
                "  256K (32K x 8)\n";
            unsigned int ch = 0;
            bool loop = true;
            while (loop) {
                ch = 0;
                HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
                switch (ch = _getch()) {
                case KEY_DOWN:
                    SetConsoleCursorPosition(output, { 0, (short)(2 + chip) });
                    std::cout << (char)(0x20) << std::flush;
                    ((chip != 1) ? chip++ : chip = 0);
                    SetConsoleCursorPosition(output, { 0, (short)(2 + chip) });
                    std::cout << ">" << std::flush;
                    SetConsoleCursorPosition(output, { 0, 4 });
                    break;
                case KEY_UP:
                    SetConsoleCursorPosition(output, { 0, (short)(2 + chip) });
                    std::cout << (char)(0x20) << std::flush;
                    ((chip != 0) ? chip-- : chip = 1);
                    SetConsoleCursorPosition(output, { 0, (short)(2 + chip) });
                    std::cout << ">" << std::flush;
                    SetConsoleCursorPosition(output, { 0, 4 });
                    break;
                case VK_RETURN:
                    loop = false;
                    break;
                }
            }
            char sdata[] = { 0x02, chip };
            size = chip ? 32768 : 8192;
            for (size_t i = 0; i < 2; i++) {
                mainmenu::write_to_serial(SP, std::string(1, sdata[i]));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            clear_screen();

            std::cout << "Reading EEPROM into buffer... ";
            mainmenu::read_into_buffer(SP);

            clear_screen();

            while (true) {
                if (!SP->IsConnected()) {
                    std::cout << "Connection lost to " << std::string("COM") + std::to_string(comPorts[sel]) << ". Press any key to reconnect... ";
                    wait_for_key_press();
                    break;
                }
                std::cout << "Select an operation to continue:\n"
                    "> Read\n"
                    "  Verify\n"
                    "  Dump to file\n"
                    "  Write\n"
                    "  Erase\n"
                    "  Set SDP\n"
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
                        ((sel != 6) ? sel++ : sel = 0);
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << ">" << std::flush;
                        SetConsoleCursorPosition(output, { 0, 8 });
                        break;
                    case KEY_UP:
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << (char)(0x20) << std::flush;
                        ((sel != 0) ? sel-- : sel = 6);
                        SetConsoleCursorPosition(output, { 0, (short)(1 + sel) });
                        std::cout << ">" << std::flush;
                        SetConsoleCursorPosition(output, { 0, 8 });
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
                case SDP:
                    mainmenu::set_sdp(SP);
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