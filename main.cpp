#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <random>

// Memory
std::uint8_t ram[4096];
std::uint8_t registry[8];
std::uint8_t I[2];
std::uint16_t pc;
std::uint8_t sp;
std::uint8_t stack[32];
std::uint8_t DT; // delay timer
std::uint8_t ST; // sound timer
std::uint16_t spriteOffset;
std::uint16_t romOffset;

// fill I register with address
void fillI(std::uint16_t addr)
{
    I[0] = (addr >> 8) & 0x00FF;
    I[1] = addr & 0x00FF;
}

// Convert I register to RAM address
std::uint16_t getAddr(std::uint8_t offset)
{
    return (I[0] << 8) + I[1] + offset;
}

// Address of font sprite X
std::uint16_t getSpriteAddr(std::uint8_t x)
{
    return ram[spriteOffset + x];
}

void initSprites()
{
    uint8_t data[80] = { 
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80
    };
    for (int i = 0; i < 80; ++i)
    {
        ram[spriteOffset + i] = data[i];
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: rom-input";
    }

    std::ifstream fin(argv[1], std::ios::binary);
    fin.read(reinterpret_cast<char*>(&ram[0x200]), 0xFFF - 0x200);
    const std::streamsize bytesRead = fin.gcount();
    std::cout << "Read " << bytesRead << " bytes from " << argv[1] << std::endl;

    spriteOffset = 0x78; // beginning of hardcoded font sprites
    initSprites();

    romOffset = 0x200;
    pc = romOffset;
    std::uint8_t instruction[2];
    while (true)
    {
        memcpy(&instruction[0], &ram[pc], 2);
        pc += 2;
        std::uint8_t opcode[4];
        opcode[0] = instruction[0] >> 4;
        opcode[1] = instruction[0] & 0x0F;
        opcode[2] = instruction[1] >> 4;
        opcode[3] = instruction[1] & 0x0F;
        std::uint8_t x = opcode[1];
        std::uint8_t y = opcode[2];
        std::uint8_t nn = instruction[1];
        std::uint16_t nnn = ((instruction[0] & 0x0F) << 8) + instruction[1];
        switch (opcode[0])
        {
        case 0x0:
        {
            switch(instruction[1])
            {
            case 0xE0: // CLS
            {
#ifdef WIN32
                system("cls");
#elif LINUX
                system("clear");
#else
                throw std::runtime_error("CLS unsupported on this platform");
#endif
            }
            break;
            case 0xEE: // RET
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            }
        }
        break;
        case 0x1:
        {
            if (nnn < romOffset || nnn > 0xFFF)
            {
                std::ostringstream oss;
                oss << "Access violation reading " << std::hex << nnn;
                throw std::runtime_error(oss.str());
            }
            pc = nnn;
        }
        break;
        case 0x2:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0x3:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0x4:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0x5:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0x6:
        {
            registry[x] = nn;
        }
        break;
        case 0x7:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0x8:
        {
            switch (opcode[3])
            {
            case 0x0:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x1:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x2:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x3:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x4:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x5:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x6:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x7:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0xE:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            }
        }
        break;
        case 0x9:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0xA:
        {
            memset(&I[0], 0x0, 2);
            memcpy(&I[0], &opcode[1], 1);
            memcpy(&I[1], &instruction[1], 2);
        }
        break;
        case 0xB:
        {
            throw std::runtime_error("Not implemented");
        }
        break;
        case 0xC:
        {
            std::random_device randomDevice;
            std::default_random_engine randomEngine(randomDevice());
            std::uniform_int_distribution<short> uniformDist(0, 255);
            uint8_t randomValue = uniformDist(randomEngine);
            randomValue = randomValue & instruction[1];
            registry[x] = randomValue;
        }
        break;
        case 0xD:
        {
            // DRW
            std::ostringstream oss;
            oss << "DRW " << std::hex << static_cast<int>(opcode[3]) << " height sprite at (" << static_cast<int>(x) << ", " << static_cast<int>(y) << ")\n";
            std::cout << oss.str();
        }
        break;
        case 0xE:
        {
            switch (instruction[1])
            {
            case 0x9E:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0xA1:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            }
        }
        break;
        case 0xF:
        {
            switch (instruction[1])
            {
            case 0x07:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x0A:
            {
                std::cin >> registry[x];
            }
            break;
            case 0x15:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x18:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x1E:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x29:
            {
                // load address of Font sprite X in register I
                std::uint16_t addr = getSpriteAddr(x);
                fillI(addr);
            }
            break;
            case 0x33:
            {
                std::uint8_t num = registry[x];
                const std::uint8_t hundreds = num / 100;
                num = num % 100;
                const std::uint8_t tens = num / 10;
                const std::uint8_t ones = num % 10;
                const std::uint16_t i = getAddr(0);
                ram[i] = hundreds;
                ram[i + 1] = tens;
                ram[i + 2] = ones;
            }
            break;
            case 0x55:
            {
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x65:
            {
                for (int i = 0; i < x; ++i)
                {
                    const std::uint16_t addr = getAddr(i);
                    registry[i] = ram[addr];
                }
            }
            break;
            }
        }
        break;
        }
    }
    std::cout << "done\n";
    return 0;
}