#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <random>
#include <SFML/Graphics.hpp>

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

std::uint16_t displayOffset;

bool waitingKey = false;
std::uint8_t keyRegistry;

const unsigned int width = 640;
const unsigned int height = 320;

std::unique_ptr<sf::RenderWindow> pWindow;
std::unique_ptr<sf::Texture> pTexture;
std::unique_ptr<sf::Sprite> pSprite;
sf::Uint8* rgba32_buffer;

void log(const std::string& msg)
{
    std::cout << msg.c_str() << std::endl;
}

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
    std::ostringstream oss;
    oss << "Get glyph index " << std::hex << static_cast<int>(x);
    log(oss.str());
    return spriteOffset + (x * 5);
}

void draw(std::uint8_t x, std::uint8_t y, std::uint8_t height)
{
    if (height > 15)
    {
        std::ostringstream oss;
        oss << "Invalid sprite height of " << static_cast<int>(height);
        throw std::runtime_error(oss.str());
    }
    if (x > 63)
    {
        std::ostringstream oss;
        oss << "Invalid sprite dx of " << static_cast<int>(x);
        throw std::runtime_error(oss.str());
    }
    if (y > 31)
    {
        std::ostringstream oss;
        oss << "Invalid sprite dy of " << static_cast<int>(y);
        throw std::runtime_error(oss.str());
    }
    std::uint16_t addr = getAddr(0);

    std::uint8_t xbyteoffset = x / 8;
    std::uint8_t bitoffset = x % 8;

    for (std::uint8_t row = 0; row < height; ++row)
    {
        std::uint8_t data = ram[addr + row];
        if (x % 8 != 0)
        {
            throw std::runtime_error("unsupported drawing command");
        }
        //std::uint8_t ypos = y + row;
        std::uint16_t dbyte = ((y + row) * 8) + (x / 8);
        std::uint16_t dOffset = displayOffset + dbyte;
        if (dOffset > 0xFFF)
        {
            throw std::runtime_error("access violation in drawing command");
        }
        ram[dOffset] ^= data;
        //for (std::uint8_t dx = 0; dx < 4; ++dx)
        //{
        //    std::uint8_t t = 0x80 >> dx;
        //    std::uint8_t xpos = x + dx;
        //    //if (data & t)
        //    //{
        //        //sf::RectangleShape rect(sf::Vector2f(10.f, 10.f));
        //        //rect.setFillColor(sf::Color::Green);
        //        //rect.setPosition(xpos * 10, ypos * 10);
        //        //pWindow->draw(rect);
        //        
        //    //}
        //}
    }
}

// block till key pressed and return the value 0-15 "0-F". or 0xFF on error
std::uint8_t getKey(sf::Event& event)
{
    if (event.type == sf::Event::KeyPressed)
    {
        sf::Keyboard::Key k = event.key.code;
        if (k >= sf::Keyboard::Num0 && k <= sf::Keyboard::Num9)
        {
            return k - sf::Keyboard::Num0;
        }
        if (k >= sf::Keyboard::Numpad0 && k <= sf::Keyboard::Numpad9)
        {
            return k - sf::Keyboard::Numpad0;
        }
        if (k >= sf::Keyboard::A && k <= sf::Keyboard::F)
        {
            return k + 10;
        }
    }
    return 0xFF;
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

void cls()
{
    memset(&ram[displayOffset], 0x00, 0x100);
}

void clearRgbaBuffer()
{
    const std::size_t bufSz = width * height * 4;
    std::size_t offset = 0x00;
    while (offset < bufSz)
    {
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0xFF;
    }
}

// these coordinates are in the screen coordinates (e.g. SFML)
// draw pixel into RGBA texture
void drawPixel(std::uint8_t x, std::uint8_t y)
{
    std::size_t offset = (y * width * 4) + x * 4;
    rgba32_buffer[offset] = 0x00;
    rgba32_buffer[offset] = 0xFF;
    rgba32_buffer[offset] = 0x00;
    rgba32_buffer[offset] = 0xFF;
}

// These coordinates are in CHIP-8 coordinates, and translates to Screen coordinates
//void drawRect(std::uint8_t x, std::uint8_t y)
//{
//    for (std::uint8_t row = 0; row < 10; ++row)
//    {
//        for (std::uint8_t col = 0; col < 10; ++col)
//        {
//            drawPixel((x * 10) + col, (y * 10) + row);
//        }
//    }
//}

// x,y are in chip 8 display coordinates
// draw on actual screen. use 10x10 pixels
// target screen is 10x chip display size
void drawScreenPixel(std::uint8_t x, std::uint8_t y)
{
    x = x * 10;
    y = y * 10;
    const std::size_t stride = width * 4;
    const std::size_t rasterLength = x + 10 > width ? x + 10 - width : 10;
    const std::size_t rowOffset = x * 4;
    for (std::uint8_t row = 0; row < 10; ++row)
    {
        std::size_t currentOffset = (y + row) * stride + rowOffset;
        for (std::uint8_t col = 0; col < rasterLength; ++col)
        {
            rgba32_buffer[currentOffset++] = 0x00;
            rgba32_buffer[currentOffset++] = 0xFF;
            rgba32_buffer[currentOffset++] = 0x00;
            rgba32_buffer[currentOffset++] = 0xFF;
        }
    }
}

void drawScreen()
{
    clearRgbaBuffer();

    std::uint16_t chip8DisplayOffset = displayOffset;
    for (std::uint8_t row = 0; row < 32; ++row)
    {
        for (std::uint8_t chunk = 0; chunk < 8; ++chunk, ++chip8DisplayOffset)
        {
            std::uint8_t data = ram[chip8DisplayOffset];
            for (std::uint8_t b = 0; b < 8; ++b)
            {
                if (data & (0x80 >> b)) drawScreenPixel((chunk * 8) + b, row);
            }
        }
    }

    pTexture->update(rgba32_buffer);
    pWindow->draw(*pSprite);
    pWindow->display();
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

    displayOffset = 0xF00; // display is from 0xF00 to 0xFFF (8 x 4 = 32 bytes). Each bit == pixel

    pWindow.reset(new sf::RenderWindow(sf::VideoMode(width, height), "CHIP-8 Interpreter"));
    pWindow->clear(sf::Color::Black);
    pTexture.reset(new sf::Texture());
    pTexture->create(width, height);
    pSprite.reset(new sf::Sprite(*pTexture));

    rgba32_buffer = new sf::Uint8[width * height * 4];
    clearRgbaBuffer();

    while (pWindow->isOpen())
    {
        sf::Event event;
        while (pWindow->pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::Closed:
            {
                pWindow->close();
            }
            break;
            case sf::Event::KeyPressed:
            {
                if (waitingKey)
                {
                    std::uint8_t k = getKey(event);
                    if (k < 0x10)
                    {
                        registry[keyRegistry] = k;
                        waitingKey = false;
                    }
                }
            }
            }
        }

        if (!waitingKey)
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
                switch (instruction[1])
                {
                case 0xE0: // CLS
                {
                    log("CLS");
                    cls();
                }
                break;
                case 0xEE: // RET
                {
                    log("RET");
                    throw std::runtime_error("Not implemented");
                }
                break;
                }
            }
            break;
            case 0x1:
            {
                log("JP addr");
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
                log("CALL addr");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x3:
            {
                log("SE Vx, byte");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x4:
            {
                log("SNE Vx, byte");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x5:
            {
                log("SE Vx, Vy");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x6:
            {
                log("LD Vx, byte");
                registry[x] = nn;
            }
            break;
            case 0x7:
            {
                log("ADD Vx, byte");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0x8:
            {
                switch (opcode[3])
                {
                case 0x0:
                {
                    log("LD Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x1:
                {
                    log("OR Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x2:
                {
                    log("AND Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x3:
                {
                    log("XOR Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x4:
                {
                    log("ADD Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x5:
                {
                    log("SUB Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x6:
                {
                    log("SHR Vx {, Vy}");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x7:
                {
                    log("SUBN Vx, Vy");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0xE:
                {
                    log("SHL Vx {, Vy}");
                    throw std::runtime_error("Not implemented");
                }
                break;
                }
            }
            break;
            case 0x9:
            {
                log("SNE Vx, Vy");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0xA:
            {
                log("LD I, addr");
                memset(&I[0], 0x0, 2);
                memcpy(&I[0], &opcode[1], 1);
                memcpy(&I[1], &instruction[1], 2);
            }
            break;
            case 0xB:
            {
                log("JP V0, addr");
                throw std::runtime_error("Not implemented");
            }
            break;
            case 0xC:
            {
                log("RND");
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
                log("DRW");
                draw(registry[x], registry[y], opcode[3]);
                // DRW
                //std::ostringstream oss;
                //oss << "DRW " << std::hex << static_cast<int>(opcode[3]) << " height sprite at (" << static_cast<int>(x) << ", " << static_cast<int>(y) << ")\n";
                //std::cout << oss.str();
            }
            break;
            case 0xE:
            {
                switch (instruction[1])
                {
                case 0x9E:
                {
                    log("SKP");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0xA1:
                {
                    log("SKNP");
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
                    log("LD Vx, DT");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x0A:
                {
                    log("LD Vx, K");
                    waitingKey = true;
                    keyRegistry = x;
                }
                break;
                case 0x15:
                {
                    log("LD DT, Vx");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x18:
                {
                    log("LD ST, Vx");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x1E:
                {
                    log("ADD I, Vx");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x29:
                {
                    log("LD F, Vx");
                    // load address of Font sprite Vx in register I
                    std::uint16_t addr = getSpriteAddr(registry[x]);
                    fillI(addr);
                }
                break;
                case 0x33:
                {
                    log("LD B, Vx");
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
                    log("LD [I], Vx");
                    throw std::runtime_error("Not implemented");
                }
                break;
                case 0x65:
                {
                    log("LD Vx, [I]");
                    for (int i = 0; i <= x; ++i)
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
        drawScreen();
    }
    std::cout << "done\n";
    pTexture.release(); // todo fix SFML mem mgmt
    return 0;
}