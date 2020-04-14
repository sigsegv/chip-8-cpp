#include <cstdarg>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <random>
#include <SFML/Graphics.hpp>

// Memory
std::uint8_t ram[0x1000];
std::uint8_t registry[16];
std::uint8_t I[2];
std::uint16_t pc;
std::uint8_t sp;
std::uint8_t stack[32];
std::uint8_t DT; // delay timer
std::uint8_t ST; // sound timer

std::uint16_t spriteOffset;
std::uint16_t romOffset;

std::uint16_t displayOffset;

std::uint16_t stackOffset;
std::uint8_t stackSize;

bool waitingKey = false;
std::uint8_t keyRegistry;

const unsigned int screenWidth = 640;
const unsigned int screenHeight = 320;

std::unique_ptr<sf::RenderWindow> pWindow;
std::unique_ptr<sf::Texture> pTexture;
std::unique_ptr<sf::Sprite> pSprite;
sf::Uint8* rgba32_buffer;

constexpr bool isBigEndian()
{
    union {
        std::uint16_t v;
        std::uint8_t b[2];
    } test = { 0xFF00 };
    return test.b[0];
}

void log(const std::string& msg)
{
    std::cout << msg.c_str() << std::endl;
}

void trace(char *fmt, ...)
{
    va_list vlist;
    va_start(vlist, fmt);
    vfprintf(stdout, fmt, vlist);
    vfprintf(stdout, "\n", vlist);
    va_end(vlist);
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

    for (std::uint8_t row = 0; row < height && y + row < 32; ++row)
    {
        const std::uint8_t spriteByte = ram[addr + row];
        const std::uint8_t remainder = x % 8;
        const std::uint8_t firstSpriteByte = spriteByte >> remainder;
        const std::uint8_t secondSpriteByte = spriteByte << (8 - remainder);

        const std::uint16_t firstOffset = ((y + row) * 8) + (x / 8) + displayOffset;
        std::uint16_t secondOffset = firstOffset + 1;
        if (x > 55)
        {
            // wrap to beginning of row
            secondOffset = (y + row) * 8;
        }

        if (firstOffset > 0xFFF)
        {
            throw std::runtime_error("access violation in drawing command");
        }
        registry[0xF] = 0x00;
        std::uint8_t testData = ram[firstOffset] & (0xFF >> remainder);
        if (testData & firstSpriteByte) registry[0xF] = 0x01;
        ram[firstOffset] ^= firstSpriteByte;
        
        if (secondOffset < 0xFFF)
        {
            if (registry[0xF] == 0x0)
            {
                testData = ram[secondOffset] & (0xFF << (8 - remainder));
                if (testData & secondSpriteByte) registry[0xF] = 0x01;
            }
            ram[secondOffset] ^= secondSpriteByte;
        }
    }
}

// return sf::Keyboard::Unkown if k not between 0x0 and 0xF
sf::Keyboard::Key chip8KeyToSfKey(std::uint8_t k)
{
    if (k < 9) return static_cast<sf::Keyboard::Key>(k + 26);
    if (k < 0x10) return static_cast<sf::Keyboard::Key>(k - 10);
    return sf::Keyboard::Unknown;
}

// return 0x0-0xF or 0xFF for invalid key
std::uint8_t sfKeyToChip8Key(sf::Keyboard::Key k)
{
    if (k >= sf::Keyboard::Num0 && k <= sf::Keyboard::Num9)
    {
        return static_cast<std::uint8_t>(k - sf::Keyboard::Num0);
    }
    if (k >= sf::Keyboard::Numpad0 && k <= sf::Keyboard::Numpad9)
    {
        return static_cast<std::uint8_t>(k - sf::Keyboard::Numpad0);
    }
    if (k >= sf::Keyboard::A && k <= sf::Keyboard::F)
    {
        return static_cast<std::uint8_t>(k + 10);
    }
    return 0xFF;
}

// get Chip8 key from key event and return as a value 0-15 "0-F". or 0xFF on invalid key
std::uint8_t getKey(sf::Event& event)
{
    if (event.type == sf::Event::KeyPressed)
    {
        sf::Keyboard::Key code = event.key.code;
        return sfKeyToChip8Key(code);
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

// clear Chip8 display buffer
void cls()
{
    memset(&ram[0xF00], 0x00, 0x100);
}

void clearRgbaBuffer()
{
    const std::size_t bufSz = screenWidth * screenHeight * 4;
    std::size_t offset = 0x00;
    while (offset < bufSz)
    {
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0x00;
        rgba32_buffer[offset++] = 0xFF;
    }
}

// x,y are in chip 8 display coordinates
// draw on actual screen. use 10x10 pixels
// target screen is 10x chip display size
void drawScreenPixel(unsigned x, unsigned y)
{
    x = x * 10;
    y = y * 10;
    if (x >= screenWidth) throw std::runtime_error("Invalid parameter");
    if (y >= screenHeight) throw std::runtime_error("Invalid parameter");
    const std::size_t stride = screenWidth * 4;
    const std::size_t rasterLength = x + 10 > screenWidth ? x + 10 - screenWidth : 10;
    const std::size_t rowOffset = x * 4;
    for (std::uint8_t row = 0; row < 10; ++row)
    {
        std::size_t currentOffset = ((y + row) * stride) + rowOffset;
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
            const std::size_t startingOffset = chunk * 8;
            const std::uint8_t data = ram[chip8DisplayOffset];
            for (std::uint8_t b = 0; b < 8; ++b)
            {
                if (data & (0x80 >> b)) drawScreenPixel(startingOffset + b, row);
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

    stackOffset = 0xEA0;

    pWindow.reset(new sf::RenderWindow(sf::VideoMode(screenWidth, screenHeight), "CHIP-8 Interpreter"));
    pWindow->clear(sf::Color::Black);
    pTexture.reset(new sf::Texture());
    pTexture->create(screenWidth, screenHeight);
    pSprite.reset(new sf::Sprite(*pTexture));

    rgba32_buffer = new sf::Uint8[screenWidth * screenHeight * 4];
    clearRgbaBuffer();

    sf::Clock clock;
    sf::Time kTimePerFrame = sf::microseconds(16666); // 60 Hertz
    sf::Time elapsedTimeSinceLastTick = sf::Time::Zero;

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
                        trace("LD Vx, K v=%X K=%X", keyRegistry, k);
                        registry[keyRegistry] = k;
                        waitingKey = false;
                    }
                }
            }
            }
        }

        elapsedTimeSinceLastTick += clock.restart();
        while (elapsedTimeSinceLastTick >= kTimePerFrame)
        {
            elapsedTimeSinceLastTick -= kTimePerFrame;
            if (DT > 0) --DT;
            if (ST > 0) --ST;
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
                    trace("CLS");
                    cls();
                }
                break;
                case 0xEE: // RET
                {
                    trace("RET");
                    if (stackSize == 0) // RCA 1802 stack limit
                    {
                        throw std::runtime_error("No function return value");
                    }
                    --stackSize;
                    pc = ram[stackOffset + (stackSize * 2)] << 8;
                    pc += ram[stackOffset + (stackSize * 2) + 1];
                }
                break;
                }
            }
            break;
            case 0x1:
            {
                //trace("JP %X", nnn);
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
                //log("CALL addr");
                trace("CALL %X", nnn);
                if (stackSize >= 12) // RCA 180 2 stack limit
                {
                    throw std::runtime_error("Stackoverflow");
                }
                ram[stackOffset + (stackSize * 2)] = (pc >> 8) & 0x00FF;
                ram[stackOffset + (stackSize * 2) + 1] = (pc & 0x00FF);
                ++stackSize;
                pc = nnn;
            }
            break;
            case 0x3:
            {
                log("SE Vx, byte");
                trace("SE Vx, byte x=%X byte=%X", x, nn);
                if (registry[x] == nn) pc += 2;
            }
            break;
            case 0x4:
            {
                //log("SNE Vx, byte");
                trace("SNE Vx, byte x=%X byte=%X", x, nn);
                if (registry[x] != nn) pc += 2;
            }
            break;
            case 0x5:
            {
                log("SE Vx, Vy");
                trace("SE Vx, Vy x=%X y=%X", x, y);
                if (registry[x] == registry[y]) pc += 2;
            }
            break;
            case 0x6:
            {
                //log("LD Vx, byte");
                trace("LD Vx, byte x=%X byte=%X", x, nn);
                registry[x] = nn;
            }
            break;
            case 0x7:
            {
                //log("ADD Vx, byte");
                trace("ADD Vx, byte x=%X byte=%X", x, nn);
                registry[x] += nn;
            }
            break;
            case 0x8:
            {
                switch (opcode[3])
                {
                case 0x0:
                {
                    //log("LD Vx, Vy");
                    trace("LD Vx, Vy x=%X y=%X", x, y);
                    registry[x] = registry[y];
                }
                break;
                case 0x1:
                {
                    //log("OR Vx, Vy");
                    registry[x] |= registry[y];
                    trace("OR Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0x2:
                {
                    //log("AND Vx, Vy");
                    registry[x] &= registry[y];
                    trace("AND Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0x3:
                {
                    //log("XOR Vx, Vy");
                    registry[x] ^= registry[y];
                    trace("XOR Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0x4:
                {
                    //log("ADD Vx, Vy");
                    std::uint16_t total = registry[x] + registry[y];
                    if (total > 0xFF) registry[0xF] = 1;
                    else  registry[0xF] = 0;
                    registry[x] += registry[y];
                    trace("ADD Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0x5:
                {
                    //log("SUB Vx, Vy");
                    if (registry[x] > registry[y]) registry[0xF] = 1;
                    else  registry[0xF] = 0;
                    registry[x] -= registry[y];
                    trace("SUB Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0x6:
                {
                    trace("SHR Vx {, Vy}");
                    if (registry[x] & 0x01) registry[0xF] = 1;
                    else  registry[0xF] = 0;
                    registry[x] = registry[x] >> 1;
                    trace("SHR Vx {, Vy} x=%X result=%X", x, registry[x]);
                }
                break;
                case 0x7:
                {
                    //log("SUBN Vx, Vy");
                    if (registry[y] > registry[x]) registry[0xF] = 1;
                    else registry[0xF] = 0;
                    registry[x] = registry[y] - registry[x];
                    trace("SUBN Vx, Vy x=%X y=%X result=%X", x, y, registry[x]);
                }
                break;
                case 0xE:
                {
                    //log("SHL Vx {, Vy}");
                    if (registry[x] & 0x80) registry[0xF] = 1;
                    else registry[0xF] = 0;
                    registry[x] = registry[x] << 2;
                    trace("SHL Vx {, Vy} x=%X result=%X", x, registry[x]);
                }
                break;
                }
            }
            break;
            case 0x9:
            {
                //log("SNE Vx, Vy");
                trace("SNE Vx, Vy x=%X y=%X Vx=%X Vy=%X", x, y, registry[x], registry[y]);
                if (registry[x] != registry[y]) pc += 2;
            }
            break;
            case 0xA:
            {
                trace("LD I, %X", nnn);
                memset(&I[0], 0x0, 2);
                memcpy(&I[0], &opcode[1], 1);
                memcpy(&I[1], &instruction[1], 2);
            }
            break;
            case 0xB:
            {
                trace("JP V0, %X", nnn);
                pc = nnn + registry[0];
            }
            break;
            case 0xC:
            {
                log("RND");
                std::random_device randomDevice;
                std::default_random_engine randomEngine(randomDevice());
                std::uniform_int_distribution<short> uniformDist(0, 255);
                uint8_t randomValue = static_cast<std::uint8_t>(uniformDist(randomEngine));
                randomValue = randomValue & instruction[1];
                registry[x] = randomValue;
            }
            break;
            case 0xD:
            {
                trace("DRW x=%X y=%X Vx=%X Vy=%X height=%X", x, y, registry[x], registry[y], opcode[3]);
                draw(registry[x], registry[y], opcode[3]);
            }
            break;
            case 0xE:
            {
                switch (instruction[1])
                {
                case 0x9E:
                {
                    trace("SKP Vx x=%X Vx=%X", x, registry[x]);
                    sf::Keyboard::Key key = chip8KeyToSfKey(registry[x]);
                    if (sf::Keyboard::isKeyPressed(key)) pc += 2;
                }
                break;
                case 0xA1:
                {
                    trace("SKNP Vx x=%X Vx=%X", x, registry[x]);
                    sf::Keyboard::Key key = chip8KeyToSfKey(registry[x]);
                    if (key != sf::Keyboard::Unknown && !sf::Keyboard::isKeyPressed(key)) pc += 2;
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
                    registry[x] = DT;
                    trace("LD Vx, DT x=%X Vx=%X", x, registry[x]);
                }
                break;
                case 0x0A:
                {
                    //log("LD Vx, K");
                    trace("getKey()");
                    waitingKey = true;
                    keyRegistry = x;
                }
                break;
                case 0x15:
                {
                    DT = registry[x];
                    trace("LD DT, Vx x=%X DT=%X", x, DT);
                }
                break;
                case 0x18:
                {
                    ST = registry[x];
                    trace("LD ST, Vx x=%X ST=%X", x, ST);
                }
                break;
                case 0x1E:
                {
                    std::uint16_t addr = getAddr(registry[x]);
                    fillI(addr);
                    trace("ADD I, Vx x=%X I=%X", x, addr);
                }
                break;
                case 0x29:
                {
                    //log("LD F, Vx");
                    // load address of Font sprite Vx in register I
                    std::uint16_t addr = getSpriteAddr(registry[x]);
                    fillI(addr);
                    trace("LD F, Vx x=%X F=%X", x, registry[x]);
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
                    trace("LD B, Vx x=%X Vx=%X addr=%X hundreds=%d tens=%d ones=%d", x, registry[x], i, hundreds, tens, ones);
                }
                break;
                case 0x55:
                {
                    if (x > 0xF) throw std::runtime_error("Invalid parameter");
                    std::uint16_t addr = getAddr(0);
                    for (int i = 0; i <= x; ++i, ++addr)
                    {
                        ram[addr] = registry[i];
                    }
                    trace("LD [I], Vx x=%X Vx=%X I=%X", x, registry[x], addr);
                }
                break;
                case 0x65:
                {
                    trace("LD Vx, [I]");
                    if (x > 0xF) throw std::runtime_error("Invalid parameter");
                    for (std::uint8_t i = 0; i <= x; ++i)
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
    trace("done");
    pTexture.reset();
    pSprite.reset();
    pWindow.reset();
    return 0;
}