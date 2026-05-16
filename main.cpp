// ============================================================
// Match-3 (3 In A Row) Game  –  SFML 2.x
// ============================================================

#include <SFML/Graphics.hpp>
#include <ctime>
#include <cmath>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

// ── Constants ───────────────────────────────────────────────
const int   GRID_SIZE   = 9;
const int   TILE_SIZE   = 65;
const int   GEM_TYPES   = 6;
const int   SPECIAL_NONE        = 0;
const int   SPECIAL_HORIZONTAL  = 1;
const int   SPECIAL_VERTICAL    = 2;
const int   SPECIAL_SQUARE      = 3;
const int   SPECIAL_COLOR       = 4;
const int   ANIM_SPEED  = 5;           // pixels moved per sub-step
const int   ANIM_STEPS  = 4;           // sub-steps per frame
const sf::Vector2i OFFSET(55, 31);     // board pixel offset in window

// Score constants
const int   SCORE_BASE    = 10;        // points per gem in a basic 3-match
const int   SCORE_EXTRA   = 5;         // bonus per gem beyond 3 in the same run
const int   COMBO_MULT    = 2;         // multiplier added per cascade level
const int   TARGET_SCORE  = 1500;      // score required to pass the level
const int   MOVE_LIMIT    = 20;        // total moves allowed for the level

// ── Piece ────────────────────────────────────────────────────
// col,row  – logical grid position (fixed per slot; never change)
// x,y      – current pixel position (animated toward col/row * TILE_SIZE)
// kind     – gem colour 0..GEM_TYPES-1   (-1 = sentinel / no gem)
// match    – set during match detection; cleared after collapse
// alpha    – 255 → 0 fade animation when matched
struct Piece {
    int x, y, col, row, kind, match, alpha, special;
    bool specialClear;
    bool specialCreated;
    bool specialActivated;
    Piece() : x(0),y(0),col(0),row(0),kind(-1),match(0),alpha(255),special(SPECIAL_NONE),specialClear(false),specialCreated(false),specialActivated(false){}
};

Piece grid[GRID_SIZE + 2][GRID_SIZE + 2];   // [0] and [GRID_SIZE+1] are sentinels

bool sameGemKind(const Piece& a, const Piece& b) {
    return a.kind >= 0 && b.kind >= 0 && a.kind == b.kind;
}

void clearSpecialLine(int row, int col, int specialType) {
    if (specialType == SPECIAL_HORIZONTAL) {
        for (int c = 1; c <= GRID_SIZE; c++)
            if (grid[row][c].kind >= 0) {
                grid[row][c].match++;
                grid[row][c].specialClear = true;
            }
    } else if (specialType == SPECIAL_VERTICAL) {
        for (int r = 1; r <= GRID_SIZE; r++)
            if (grid[r][col].kind >= 0) {
                grid[r][col].match++;
                grid[r][col].specialClear = true;
            }
    }
}

void clearSpecialArea(int row, int col) {
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int r = row + dr;
            int c = col + dc;
            if (r >= 1 && r <= GRID_SIZE && c >= 1 && c <= GRID_SIZE && grid[r][c].kind >= 0) {
                grid[r][c].match++;
                grid[r][c].specialClear = true;
            }
        }
    }
}

void clearSameColor(int row, int col) {
    int kind = grid[row][col].kind;
    if (kind < 0) return;
    for (int i = 1; i <= GRID_SIZE; i++) {
        for (int j = 1; j <= GRID_SIZE; j++) {
            if (grid[i][j].kind == kind) {
                grid[i][j].match++;
                grid[i][j].specialClear = true;
            }
        }
    }
}

void clearCross(int row, int col) {
    for (int dr = -1; dr <= 1; dr++) {
        int r = row + dr;
        if (r < 1 || r > GRID_SIZE) continue;
        for (int c = 1; c <= GRID_SIZE; c++) {
            if (grid[r][c].kind >= 0) {
                grid[r][c].match++;
                grid[r][c].specialClear = true;
            }
        }
    }
    for (int dc = -1; dc <= 1; dc++) {
        int c = col + dc;
        if (c < 1 || c > GRID_SIZE) continue;
        for (int r = 1; r <= GRID_SIZE; r++) {
            if (grid[r][c].kind >= 0) {
                grid[r][c].match++;
                grid[r][c].specialClear = true;
            }
        }
    }
}

void clearLineCross(int row, int col) {
    if (row >= 1 && row <= GRID_SIZE) {
        for (int c = 1; c <= GRID_SIZE; c++) {
            if (grid[row][c].kind >= 0) {
                grid[row][c].match++;
                grid[row][c].specialClear = true;
            }
        }
    }
    if (col >= 1 && col <= GRID_SIZE) {
        for (int r = 1; r <= GRID_SIZE; r++) {
            if (grid[r][col].kind >= 0) {
                grid[r][col].match++;
                grid[r][col].specialClear = true;
            }
        }
    }
}

// Blast + Line combo: clears 3 full rows and 3 full columns centred on (row,col).
void clearTripleCross(int row, int col) {
    for (int dr = -1; dr <= 1; dr++) {
        int r = row + dr;
        if (r < 1 || r > GRID_SIZE) continue;
        for (int c = 1; c <= GRID_SIZE; c++) {
            if (grid[r][c].kind >= 0) {
                grid[r][c].match++;
                grid[r][c].specialClear = true;
            }
        }
    }
    for (int dc = -1; dc <= 1; dc++) {
        int c = col + dc;
        if (c < 1 || c > GRID_SIZE) continue;
        for (int r = 1; r <= GRID_SIZE; r++) {
            if (grid[r][c].kind >= 0) {
                grid[r][c].match++;
                grid[r][c].specialClear = true;
            }
        }
    }
}

// ── Score popup ──────────────────────────────────────────────
// A small floating "+N" label that rises and fades over ~1 second.
struct ScorePopup {
    sf::Text text;
    float    x, y;       // current pixel position
    float    vy;         // vertical velocity (pixels/frame, upward = negative)
    int      life;       // frames remaining
    bool     active;
    ScorePopup() : x(0),y(0),vy(-1.5f),life(0),active(false){}
};

// ── Level definition ─────────────────────────────────────────
struct Level {
    std::string name;
    int target;
    int moves;
    bool completed;
};

// ── swapForPlayer ────────────────────────────────────────────
// Swap gem visuals between two adjacent cells and cross their pixel
// start positions so the animation system slides them across.
void swapForPlayer(Piece& a, Piece& b) {
    std::swap(a.x,     b.x);
    std::swap(a.y,     b.y);
    std::swap(a.kind,  b.kind);
    std::swap(a.alpha, b.alpha);
    std::swap(a.special, b.special);
    std::swap(a.specialClear, b.specialClear);
    std::swap(a.specialCreated, b.specialCreated);
    std::swap(a.specialActivated, b.specialActivated);
}

// ── Formatted score string ───────────────────────────────────
std::string fmtScore(int n) {
    // Insert thousands separators  e.g. 12345 → "12,345"
    std::string s = std::to_string(n);
    int ins = static_cast<int>(s.size()) - 3;
    while (ins > 0) { s.insert(ins, ","); ins -= 3; }
    return s;
}

// ── main ─────────────────────────────────────────────────────
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    sf::RenderWindow window(sf::VideoMode(1024, 768), "Match-3 Game", sf::Style::Close);
    window.setFramerateLimit(75);

    // ── Textures ─────────────────────────────────────────────
    sf::Texture bgTex, gemsTex, boardTex, horizTex, vertTex, squareTex, colorTex;
    if (!bgTex.loadFromFile("images/background.jpg"))   { std::cerr << "Missing background.jpg\n";  return 1; }
    if (!gemsTex.loadFromFile("images/gems!.png"))      { std::cerr << "Missing gems!.png\n";       return 1; }
    if (!horizTex.loadFromFile("images/horizontal_gems!.png")) { std::cerr << "Missing horizontal_gems!.png\n"; return 1; }
    if (!vertTex.loadFromFile("images/vertical_gems!.png"))   { std::cerr << "Missing vertical_gems!.png\n";   return 1; }
    if (!squareTex.loadFromFile("images/2x2_gems!.png"))   { std::cerr << "Missing 2x2_gems!.png\n";   return 1; }
    if (!colorTex.loadFromFile("images/5inrow_gems!.png")) { std::cerr << "Missing 5inrow_gems!.png\n"; return 1; }
    if (!boardTex.loadFromFile("images/board.png"))     { std::cerr << "Missing board.png\n";       return 1; }
    bgTex.setSmooth(true);
    gemsTex.setSmooth(true);
    horizTex.setSmooth(true);
    vertTex.setSmooth(true);
    squareTex.setSmooth(true);
    colorTex.setSmooth(true);

    // Determine which texture actually contains the horizontal (left-right arrow) gem
    // by checking which file's frame is wider than it is tall.
    // horizontal_gems!.png frames are square 416x416 with a left-right arrow icon.
    // vertical_gems!.png frames are square 170x170 with an up-down arrow icon.
    // We assign by content: the file whose frame is larger holds the horizontal special.
    int horizFrameWidth = horizTex.getSize().x / GEM_TYPES;
    int vertFrameWidth  = vertTex.getSize().x / GEM_TYPES;
    // If the files are swapped (vertical file has larger frames), fix the assignment.
    bool swapSpecialTextures = (vertFrameWidth > horizFrameWidth);

    const sf::Texture& horizontalSpecialTex = swapSpecialTextures ? vertTex : horizTex;
    const sf::Texture& verticalSpecialTex   = swapSpecialTextures ? horizTex : vertTex;
    const sf::Texture& squareSpecialTex     = squareTex;
    const sf::Texture& colorSpecialTex      = colorTex;
    const int HORIZ_FRAME_WIDTH = horizontalSpecialTex.getSize().x / GEM_TYPES;
    const int HORIZ_FRAME_HEIGHT = horizontalSpecialTex.getSize().y;
    const int VERT_FRAME_WIDTH = 166; // use the requested vertical special gem frame width
    const int VERT_FRAME_HEIGHT = verticalSpecialTex.getSize().y;
    const int SQUARE_FRAME_WIDTH = squareSpecialTex.getSize().x / GEM_TYPES;
    const int SQUARE_FRAME_HEIGHT = squareSpecialTex.getSize().y;
    const int COLOR_FRAME_WIDTH = colorSpecialTex.getSize().x / GEM_TYPES;
    const int COLOR_FRAME_HEIGHT = colorSpecialTex.getSize().y;

    sf::Sprite background(bgTex);
    sf::Sprite gems(gemsTex);

    sf::Sprite board(boardTex);
    board.setColor(sf::Color(255,255,255,128));
    board.setPosition(48.f, 24.f);
    board.setScale(3.4f, 3.4f);

    // ── Font & score UI ──────────────────────────────────────
    sf::Font font;
    bool fontLoaded = font.loadFromFile("images/arial.ttf");
    if (!fontLoaded) {
        const char* fallbackFonts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
        };
        for (const char* path : fallbackFonts) {
            if (font.loadFromFile(path)) {
                fontLoaded = true;
                break;
            }
        }
    }

    // Score panel background (semi-transparent dark rectangle on right side)
    sf::RectangleShape scorePanel(sf::Vector2f(330.f, 290.f));
    scorePanel.setFillColor(sf::Color(0, 0, 0, 180));
    scorePanel.setOutlineColor(sf::Color(220, 220, 220, 200));
    scorePanel.setOutlineThickness(2.f);
    scorePanel.setPosition(700.f, 20.f);

    // Helper: make a styled sf::Text
    auto makeText = [&](unsigned size, sf::Color fill) -> sf::Text {
        sf::Text t;
        if (fontLoaded) {
            t.setFont(font);
            t.setCharacterSize(size);
            t.setFillColor(fill);
            t.setOutlineColor(sf::Color::Black);
            t.setOutlineThickness(1.5f);
        }
        return t;
    };

    sf::Text labelScore   = makeText(20, sf::Color(180,180,180));
    sf::Text valueScore   = makeText(38, sf::Color(255,220, 50));   // gold
    sf::Text labelTarget  = makeText(20, sf::Color(180,180,180));
    sf::Text valueTarget  = makeText(34, sf::Color(160,255,140));   // mint
    sf::Text labelCombo   = makeText(20, sf::Color(180,180,180));
    sf::Text valueCombo   = makeText(32, sf::Color(255,120, 60));   // orange
    sf::Text labelMoves   = makeText(20, sf::Color(180,180,180));
    sf::Text valueMoves   = makeText(32, sf::Color(180,255,180));   // light green
    sf::Text statusText   = makeText(24, sf::Color(180,255,180));
    sf::Text titleText    = makeText(48, sf::Color(255,240,180));
    sf::Text promptText   = makeText(24, sf::Color(220,220,255));
    sf::Text levelHeader  = makeText(28, sf::Color(240,240,200));
    sf::Text levelPrompt  = makeText(20, sf::Color(200,200,255));
    std::vector<Level> levels = {
        {"Easy",      800, 25, false},
        {"Medium",   1500, 20, false},
        {"Hard",     2200, 15, false},
        {"Unlimited", -1, -1, false}
    };
    std::vector<sf::Text> levelOptionText(levels.size());
    sf::RectangleShape menuOverlay(sf::Vector2f(1024.f, 768.f));
    menuOverlay.setFillColor(sf::Color(0, 0, 0, 200));

    sf::Text pauseTitle = makeText(36, sf::Color(255,255,255));
    std::vector<sf::Text> pauseOptionText(3);
    std::vector<std::string> pauseOptions = {"Continue", "Restart", "Back to menu"};
    sf::RectangleShape pauseOverlay(sf::Vector2f(1024.f, 768.f));
    pauseOverlay.setFillColor(sf::Color(0, 0, 0, 180));

    if (fontLoaded) {
        titleText.setString("3 IN A ROW");
        titleText.setPosition((1024.f - titleText.getGlobalBounds().width) / 2.f, 220.f);
        promptText.setString("Press ENTER to choose a level");
        promptText.setPosition((1024.f - promptText.getGlobalBounds().width) / 2.f, 300.f);

        levelHeader.setString("Choose a Level");
        levelHeader.setPosition((1024.f - levelHeader.getGlobalBounds().width) / 2.f, 220.f);
        levelPrompt.setString("Use UP/DOWN and ENTER to select");
        levelPrompt.setPosition((1024.f - levelPrompt.getGlobalBounds().width) / 2.f, 270.f);

        labelScore.setString("SCORE");
        labelScore.setPosition(720.f, 30.f);
        valueScore.setPosition(720.f, 56.f);

        labelTarget.setString("TARGET");
        labelTarget.setPosition(720.f, 110.f);
        valueTarget.setPosition(720.f, 136.f);

        statusText.setPosition(720.f, 174.f);

        labelCombo.setString("COMBO");
        labelCombo.setPosition(720.f, 220.f);
        valueCombo.setPosition(720.f, 244.f);

        labelMoves.setString("MOVES LEFT");
        labelMoves.setPosition(870.f, 220.f);
        valueMoves.setPosition(870.f, 244.f);

        for (size_t i = 0; i < levels.size(); ++i) {
            std::string moveLabel = (levels[i].moves < 0) ? "unlimited" : std::to_string(levels[i].moves) + " moves";
            std::string text = levels[i].name + " - " + fmtScore(levels[i].target) + " pts, " + moveLabel;
            levelOptionText[i] = makeText(24, sf::Color(220,220,255));
            levelOptionText[i].setString(text);
            levelOptionText[i].setPosition((1024.f - levelOptionText[i].getGlobalBounds().width) / 2.f, 330.f + i * 50.f);
        }

        pauseTitle.setString("PAUSED");
        pauseTitle.setPosition((1024.f - pauseTitle.getGlobalBounds().width) / 2.f, 180.f);
        for (size_t i = 0; i < pauseOptionText.size(); ++i) {
            pauseOptionText[i] = makeText(28, sf::Color(220,220,255));
            pauseOptionText[i].setString(pauseOptions[i]);
            pauseOptionText[i].setPosition((1024.f - pauseOptionText[i].getGlobalBounds().width) / 2.f, 260.f + i * 50.f);
        }
    }

    // Score popups pool
    const int MAX_POPUPS = 16;
    ScorePopup popups[MAX_POPUPS];
    if (fontLoaded)
        for (auto& p : popups) {
            p.text = makeText(22, sf::Color(255,255,80));
        }

    auto spawnPopup = [&](int pts, float wx, float wy) {
        if (!fontLoaded) return;
        for (auto& p : popups) {
            if (!p.active) {
                p.x = wx;  p.y = wy;
                p.vy = -1.8f;
                p.life = 55;
                p.active = true;
                p.text.setString("+" + fmtScore(pts));
                p.text.setFillColor(sf::Color(255, 220, 50));
                break;
            }
        }
    };

    // ── Game state ───────────────────────────────────────────
    int  totalScore     = 0;
    int  combo          = 0;      // cascade level (resets on player input)
    int  movesLeft      = MOVE_LIMIT; // remaining valid swaps
    int  currentTarget  = TARGET_SCORE;
    int  currentLevel   = 0;
    bool levelPassed    = false;
    bool gameOver       = false;
    bool inMenu         = true;
    bool inPauseMenu    = false;
    bool selectingLevel = false;
    int  pauseSelection = 0;

    // ── Input state ──────────────────────────────────────────
    int  x0=0, y0=0, x=0, y=0;
    int  click    = 0;
    bool isSwap   = false;
    bool isMoving = false;
    bool savedSwapSpecialCreatedA = false;
    bool savedSwapSpecialCreatedB = false;
    bool savedSwapSpecialActivatedA = false;
    bool savedSwapSpecialActivatedB = false;
    sf::Vector2i mousePos;

    auto startLevel = [&](int levelIndex) {
        totalScore = 0;
        combo = 0;
        movesLeft = levels[levelIndex].moves;
        currentTarget = levels[levelIndex].target;
        levelPassed = false;
        gameOver = false;
        inPauseMenu = false;
        pauseSelection = 0;
        click = 0;
        isSwap = false;
        isMoving = false;

        for (int i = 1; i <= GRID_SIZE; i++) {
            for (int j = 1; j <= GRID_SIZE; j++) {
                int k;
                do {
                    k = rand() % GEM_TYPES;
                } while (
                    (j >= 3 && grid[i][j-1].kind == k && grid[i][j-2].kind == k) ||
                    (i >= 3 && grid[i-1][j].kind == k && grid[i-2][j].kind == k));
                grid[i][j] = Piece();
                grid[i][j].kind = k;
                grid[i][j].special = SPECIAL_NONE;
                grid[i][j].col  = j;
                grid[i][j].row  = i;
                grid[i][j].x    = j * TILE_SIZE;
                grid[i][j].y    = i * TILE_SIZE;
                grid[i][j].alpha = 255;
            }
        }
    };

    // ── Initialise grid ──────────────────────────────────────
    startLevel(currentLevel);

    // ═════════════════════════════════════════════════════════
    while (window.isOpen()) {

        // ── 1. Events ────────────────────────────────────────
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::KeyPressed) {
                if (inMenu) {
                    if (!selectingLevel && (e.key.code == sf::Keyboard::Return || e.key.code == sf::Keyboard::Space)) {
                        selectingLevel = true;
                    } else if (selectingLevel) {
                        if (e.key.code == sf::Keyboard::Up) {
                            currentLevel = (currentLevel + static_cast<int>(levels.size()) - 1) % static_cast<int>(levels.size());
                        } else if (e.key.code == sf::Keyboard::Down) {
                            currentLevel = (currentLevel + 1) % static_cast<int>(levels.size());
                        } else if (e.key.code == sf::Keyboard::Return || e.key.code == sf::Keyboard::Space) {
                            inMenu = false;
                            selectingLevel = false;
                            totalScore = 0;
                            combo = 0;
                            movesLeft = levels[currentLevel].moves;
                            currentTarget = levels[currentLevel].target;
                            levelPassed = false;
                            gameOver = false;
                            inPauseMenu = false;
                            pauseSelection = 0;
                        }
                    }
                } else if (inPauseMenu) {
                    if (e.key.code == sf::Keyboard::Up) {
                        pauseSelection = (pauseSelection + static_cast<int>(pauseOptionText.size()) - 1) % static_cast<int>(pauseOptionText.size());
                    } else if (e.key.code == sf::Keyboard::Down) {
                        pauseSelection = (pauseSelection + 1) % static_cast<int>(pauseOptionText.size());
                    } else if (e.key.code == sf::Keyboard::Return || e.key.code == sf::Keyboard::Space) {
                        if (pauseSelection == 0) {
                            inPauseMenu = false;
                        } else if (pauseSelection == 1) {
                            startLevel(currentLevel);
                        } else if (pauseSelection == 2) {
                            inMenu = true;
                            selectingLevel = false;
                            inPauseMenu = false;
                        }
                    } else if (e.key.code == sf::Keyboard::Escape) {
                        inPauseMenu = false;
                    }
                } else {
                    if (e.key.code == sf::Keyboard::Escape && !levelPassed && !gameOver) {
                        inPauseMenu = true;
                        pauseSelection = 0;
                    }
                }
            }
            if (e.type == sf::Event::MouseButtonPressed &&
                e.mouseButton.button == sf::Mouse::Left)
            {
                if (inMenu) {
                    if (!selectingLevel) {
                        selectingLevel = true;
                    } else {
                        sf::Vector2f clickPos(static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y));
                        for (size_t i = 0; i < levelOptionText.size(); ++i) {
                            if (levelOptionText[i].getGlobalBounds().contains(clickPos)) {
                                currentLevel = static_cast<int>(i);
                                inMenu = false;
                                selectingLevel = false;
                                totalScore = 0;
                                combo = 0;
                                movesLeft = levels[currentLevel].moves;
                                currentTarget = levels[currentLevel].target;
                                levelPassed = false;
                                gameOver = false;
                                break;
                            }
                        }
                    }
                } else if (!isSwap && !isMoving && !levelPassed && !gameOver && !inPauseMenu) {
                    click++;
                    mousePos = sf::Mouse::getPosition(window) - OFFSET;
                }
            }
        }

        if (inMenu) {
            window.draw(background);
            window.draw(menuOverlay);
            if (!selectingLevel) {
                window.draw(titleText);
                window.draw(promptText);
            } else {
                window.draw(levelHeader);
                window.draw(levelPrompt);
                for (size_t i = 0; i < levelOptionText.size(); ++i) {
                    std::string text;
                    if (levels[i].target < 0) {
                        text = levels[i].name + " - free play";
                    } else {
                        std::string moveLabel = (levels[i].moves < 0) ? "unlimited" : std::to_string(levels[i].moves) + " moves";
                        text = levels[i].name + " - " + fmtScore(levels[i].target) + " pts, " + moveLabel;
                    }
                    levelOptionText[i].setString(text);
                    levelOptionText[i].setPosition((1024.f - levelOptionText[i].getGlobalBounds().width) / 2.f, 330.f + i * 50.f);
                    if (i == static_cast<size_t>(currentLevel)) {
                        levelOptionText[i].setFillColor(sf::Color(255,255,160));
                    } else if (levels[i].completed) {
                        levelOptionText[i].setFillColor(sf::Color(140,255,140));
                    } else {
                        levelOptionText[i].setFillColor(sf::Color(220,220,255));
                    }
                    window.draw(levelOptionText[i]);
                }
            }
            window.display();
            continue;
        }

        // ── 2. Two-click swap ─────────────────────────────────
        if (click == 1) {
            x0 = mousePos.x / TILE_SIZE + 1;
            y0 = mousePos.y / TILE_SIZE + 1;
            if (x0 < 1 || x0 > GRID_SIZE || y0 < 1 || y0 > GRID_SIZE) click = 0;
        }
        if (click == 2) {
            x = mousePos.x / TILE_SIZE + 1;
            y = mousePos.y / TILE_SIZE + 1;
            if (x < 1 || x > GRID_SIZE || y < 1 || y > GRID_SIZE) {
                click = 0;
            } else if (abs(x-x0) + abs(y-y0) == 1) {
                savedSwapSpecialCreatedA = grid[y0][x0].specialCreated;
                savedSwapSpecialCreatedB = grid[y][x].specialCreated;
                savedSwapSpecialActivatedA = grid[y0][x0].specialActivated;
                savedSwapSpecialActivatedB = grid[y][x].specialActivated;
                bool activateA = grid[y0][x0].special == SPECIAL_COLOR && !grid[y0][x0].specialCreated;
                bool activateB = grid[y][x].special == SPECIAL_COLOR && !grid[y][x].specialCreated;
                swapForPlayer(grid[y0][x0], grid[y][x]);
                grid[y0][x0].specialCreated = false;
                grid[y][x].specialCreated = false;
                grid[y0][x0].specialActivated = activateB;
                grid[y][x].specialActivated = activateA;
                isSwap = true;  click = 0;
                combo  = 0;     // player move resets cascade counter
            } else {
                click = 1;  x0 = x;  y0 = y;
            }
        }

        // ── 3. Slide animation ────────────────────────────────
        isMoving = false;
        for (int i = 1; i <= GRID_SIZE; i++) {
            for (int j = 1; j <= GRID_SIZE; j++) {
                Piece& p = grid[i][j];
                for (int s = 0; s < ANIM_STEPS; s++) {
                    int dx = p.x - p.col * TILE_SIZE;
                    int dy = p.y - p.row * TILE_SIZE;
                    if (dx) p.x -= dx / abs(dx);
                    if (dy) p.y -= dy / abs(dy);
                }
                if (p.x != p.col * TILE_SIZE || p.y != p.row * TILE_SIZE)
                    isMoving = true;
            }
        }

        // ── 4. Match detection (settled board only) ───────────
        if (!isMoving) {
            for (int i = 1; i <= GRID_SIZE; i++)
                for (int j = 1; j <= GRID_SIZE; j++) {
                    grid[i][j].match = 0;
                    grid[i][j].specialClear = false;
                }

            int specialGrid[GRID_SIZE + 2][GRID_SIZE + 2] = {};

            // Detect horizontal runs and create horizontal specials for 4+ matches.
            for (int i = 1; i <= GRID_SIZE; i++) {
                int j = 1;
                while (j <= GRID_SIZE) {
                    if (grid[i][j].kind < 0) { j++; continue; }
                    int start = j;
                    int kind = grid[i][j].kind;
                    while (j + 1 <= GRID_SIZE && grid[i][j + 1].kind == kind) j++;
                    int len = j - start + 1;
                    if (len >= 3) {
                        for (int k = start; k <= j; k++)
                            grid[i][k].match++;
                        if (len >= 4) {
                            int specialCol = start + (len - 1) / 2;
                            specialGrid[i][specialCol] = (len >= 5 ? SPECIAL_COLOR : SPECIAL_HORIZONTAL);
                        }
                    }
                    j++;
                }
            }

            // Detect vertical runs and create vertical specials for 4+ matches.
            for (int j = 1; j <= GRID_SIZE; j++) {
                int i = 1;
                while (i <= GRID_SIZE) {
                    if (grid[i][j].kind < 0) { i++; continue; }
                    int start = i;
                    int kind = grid[i][j].kind;
                    while (i + 1 <= GRID_SIZE && grid[i + 1][j].kind == kind) i++;
                    int len = i - start + 1;
                    if (len >= 3) {
                        for (int k = start; k <= i; k++)
                            grid[k][j].match++;
                        if (len >= 4) {
                            int specialRow = start + (len - 1) / 2;
                            specialGrid[specialRow][j] = (len >= 5 ? SPECIAL_COLOR : SPECIAL_VERTICAL);
                        }
                    }
                    i++;
                }
            }

            // Detect 2x2 block matches and create square specials.
            for (int i = 1; i < GRID_SIZE; i++) {
                for (int j = 1; j < GRID_SIZE; j++) {
                    int kind = grid[i][j].kind;
                    if (kind >= 0 &&
                        grid[i][j+1].kind == kind &&
                        grid[i+1][j].kind == kind &&
                        grid[i+1][j+1].kind == kind) {
                        if (specialGrid[i+1][j+1] == SPECIAL_NONE) {
                            specialGrid[i+1][j+1] = SPECIAL_SQUARE;
                            grid[i][j].match++;
                            grid[i][j+1].match++;
                            grid[i+1][j].match++;
                        }
                    }
                }
            }

            // Create special gems where 4+ runs occurred.
            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    if (specialGrid[i][j] != SPECIAL_NONE) {
                        grid[i][j].match = 0;
                        grid[i][j].special = specialGrid[i][j];
                        grid[i][j].specialCreated = true;
                    }
                }
            }

            auto isLineSpecial = [&](int special) {
                return special == SPECIAL_HORIZONTAL || special == SPECIAL_VERTICAL;
            };
            auto isBoomSpecial = [&](int special) {
                return special == SPECIAL_SQUARE;
            };

            // Special gem pair activation: two adjacent specials activate without needing a third gem.
            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j < GRID_SIZE; j++) {
                    int s1 = grid[i][j].special;
                    int s2 = grid[i][j + 1].special;
                    if (s1 == SPECIAL_NONE || s2 == SPECIAL_NONE) continue;
                    if (grid[i][j].specialCreated || grid[i][j + 1].specialCreated) continue;
                    if (isLineSpecial(s1) && isLineSpecial(s2)) {
                        grid[i][j].match++;
                        grid[i][j + 1].match++;
                        clearLineCross(i, j);
                    } else if (isBoomSpecial(s1) && isBoomSpecial(s2)) {
                        grid[i][j].match++;
                        grid[i][j + 1].match++;
                    } else if ((isLineSpecial(s1) && isBoomSpecial(s2)) || (isBoomSpecial(s1) && isLineSpecial(s2))) {
                        grid[i][j].match++;
                        grid[i][j + 1].match++;
                        // Clear 3 full rows + 3 full columns centred on the boom gem
                        int boomCol = isBoomSpecial(s1) ? j : j + 1;
                        clearTripleCross(i, boomCol);
                    }
                }
            }
            for (int j = 1; j <= GRID_SIZE; j++) {
                for (int i = 1; i < GRID_SIZE; i++) {
                    int s1 = grid[i][j].special;
                    int s2 = grid[i + 1][j].special;
                    if (s1 == SPECIAL_NONE || s2 == SPECIAL_NONE) continue;
                    if (grid[i][j].specialCreated || grid[i + 1][j].specialCreated) continue;
                    if (isLineSpecial(s1) && isLineSpecial(s2)) {
                        grid[i][j].match++;
                        grid[i + 1][j].match++;
                        clearLineCross(i, j);
                    } else if (isBoomSpecial(s1) && isBoomSpecial(s2)) {
                        grid[i][j].match++;
                        grid[i + 1][j].match++;
                    } else if ((isLineSpecial(s1) && isBoomSpecial(s2)) || (isBoomSpecial(s1) && isLineSpecial(s2))) {
                        grid[i][j].match++;
                        grid[i + 1][j].match++;
                        // Clear 3 full rows + 3 full columns centred on the boom gem
                        int boomRow = isBoomSpecial(s1) ? i : i + 1;
                        clearTripleCross(boomRow, j);
                    }
                }
            }

            // Activate color-clear specials when they are adjacent to a single same-colored gem.
            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    if (grid[i][j].special == SPECIAL_COLOR && grid[i][j].kind >= 0 && !grid[i][j].specialCreated && grid[i][j].specialActivated) {
                        grid[i][j].match++;
                        clearSameColor(i, j);
                        grid[i][j].specialActivated = false;
                    }
                }
            }

            // Activate special clears for matched special gems.
            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    if (grid[i][j].match && grid[i][j].special == SPECIAL_HORIZONTAL)
                        clearSpecialLine(i, j, SPECIAL_HORIZONTAL);
                    else if (grid[i][j].match && grid[i][j].special == SPECIAL_VERTICAL)
                        clearSpecialLine(i, j, SPECIAL_VERTICAL);
                    else if (grid[i][j].match && grid[i][j].special == SPECIAL_SQUARE)
                        clearSpecialArea(i, j);
                }
            }
        }

        // ── 5. Fade matched gems & award score ───────────────
        if (!isMoving) {
            bool anyFading = false;

            // Count matched gems and compute points on the very first fade frame
            // (alpha == 255).  Combo multiplier increases each cascade level.
            int matchCount = 0;
            for (int i = 1; i <= GRID_SIZE; i++)
                for (int j = 1; j <= GRID_SIZE; j++)
                    if (grid[i][j].match && grid[i][j].alpha == 255)
                        matchCount++;

            if (matchCount > 0) {
                combo++;   // each wave of matches increments cascade level

                int pts = 30 * matchCount * (1 + (combo - 1) * COMBO_MULT);
                totalScore += pts;

                // Spawn a popup near the centre of the matched gems
                float cx = 0, cy = 0;
                int   cnt = 0;
                for (int i = 1; i <= GRID_SIZE; i++)
                    for (int j = 1; j <= GRID_SIZE; j++)
                        if (grid[i][j].match && grid[i][j].alpha == 255) {
                            cx += grid[i][j].x + OFFSET.x - TILE_SIZE + 32;
                            cy += grid[i][j].y + OFFSET.y - TILE_SIZE;
                            cnt++;
                        }
                if (cnt > 0) spawnPopup(pts, cx/cnt, cy/cnt);
            }

            // Step the fade
            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    if (grid[i][j].match) {
                        if (grid[i][j].alpha > 10) {
                            grid[i][j].alpha -= 10;
                            anyFading = true;
                        }
                    }
                }
            }
            if (anyFading) isMoving = true;
        }

        // ── 6. Revert invalid swap ────────────────────────────
        if (isSwap && !isMoving) {
            bool anyMatch = false;
            for (int i = 1; i <= GRID_SIZE && !anyMatch; i++)
                for (int j = 1; j <= GRID_SIZE && !anyMatch; j++)
                    if (grid[i][j].match) anyMatch = true;
            if (!anyMatch) {
                swapForPlayer(grid[y0][x0], grid[y][x]);
                grid[y0][x0].specialCreated = savedSwapSpecialCreatedA;
                grid[y][x].specialCreated = savedSwapSpecialCreatedB;
                grid[y0][x0].specialActivated = savedSwapSpecialActivatedA;
                grid[y][x].specialActivated = savedSwapSpecialActivatedB;
                combo = 0;
            } else {
                if (levels[currentLevel].moves >= 0) {
                    movesLeft = std::max(0, movesLeft - 1);
                }
            }
            isSwap = false;
        }

        // ── 7. Collapse & refill with fall animation ──────────
        //
        // KEY FIX: survivors don't teleport — each survivor's pixel y is set
        // to where it CURRENTLY IS (its old row * TILE_SIZE) so it slides
        // smoothly down to its new row.
        //
        // For each column j:
        //  a) Walk bottom-up, collect (kind, currentPixelY) of survivors.
        //  b) Write survivors back from the bottom; give each one its old
        //     pixel y as the starting y so it falls the right distance.
        //  c) Fill empty top slots with new gems starting above the board.
        if (!isMoving) {
            bool anyMatched = false;
            for (int i = 1; i <= GRID_SIZE && !anyMatched; i++)
                for (int j = 1; j <= GRID_SIZE && !anyMatched; j++)
                    if (grid[i][j].match) anyMatched = true;

            if (anyMatched) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    // Collect survivors (kind + current pixel y) from bottom up
                    struct Survivor { int kind; int special; bool specialClear; int pixelY; };
                    Survivor surv[GRID_SIZE];
                    int count = 0;
                    for (int i = GRID_SIZE; i >= 1; i--)
                        if (!grid[i][j].match)
                            surv[count++] = { grid[i][j].kind, grid[i][j].special, grid[i][j].specialClear, grid[i][j].y };

                    // Write slots from the bottom of the column upward
                    for (int i = GRID_SIZE; i >= 1; i--) {
                        int fromBottom = GRID_SIZE - i;  // 0 = bottom row

                        if (fromBottom < count) {
                            // Survivor: slide from its old pixel y down to new row
                            grid[i][j].kind         = surv[fromBottom].kind;
                            grid[i][j].special      = surv[fromBottom].special;
                            grid[i][j].specialClear = surv[fromBottom].specialClear;
                            grid[i][j].alpha        = 255;
                            grid[i][j].match        = 0;
                            // Start at old pixel position → animation slides it down
                            grid[i][j].x = j * TILE_SIZE;
                            grid[i][j].y = surv[fromBottom].pixelY;
                            // Target is col/row * TILE_SIZE (already set via col,row)
                        } else {
                            // New gem: spawn above the board, fall into position
                            int aboveIndex = fromBottom - count; // 0 = closest above board
                            grid[i][j].kind      = rand() % GEM_TYPES;
                            grid[i][j].special   = SPECIAL_NONE;
                            grid[i][j].specialClear = false;
                            grid[i][j].alpha     = 255;
                            grid[i][j].match     = 0;
                            grid[i][j].x         = j * TILE_SIZE;
                            // Stack new gems above the board top, spaced one tile apart
                            grid[i][j].y         = -(aboveIndex + 1) * TILE_SIZE;
                        }
                    }
                }
            }
        }

        // ── 8. Update score popups ────────────────────────────
        for (auto& p : popups) {
            if (!p.active) continue;
            p.y += p.vy;
            p.life--;
            if (p.life <= 0) { p.active = false; continue; }
            // Fade out in the last 20 frames
            sf::Uint8 a = (p.life < 20)
                ? static_cast<sf::Uint8>(255 * p.life / 20)
                : 255;
            sf::Color c = p.text.getFillColor();
            c.a = a;
            p.text.setFillColor(c);
            p.text.setPosition(p.x, p.y);
        }

        // ── 9. Draw ───────────────────────────────────────────
        window.draw(background);
        window.draw(board);

        // Clip gem drawing to the board area so falling gems spawning above
        // the grid are not visible.  Switch to a viewport matching the board
        // rectangle, draw all gems, then restore the default full-window view.
        {
            const float WIN_W = 1024.f, WIN_H = 768.f;
            const float boardLeft = static_cast<float>(OFFSET.x);
            const float boardTop  = static_cast<float>(OFFSET.y);
            const float boardW    = static_cast<float>(GRID_SIZE * TILE_SIZE);
            const float boardH    = static_cast<float>(GRID_SIZE * TILE_SIZE);

            sf::View clipView(sf::FloatRect(boardLeft, boardTop, boardW, boardH));
            clipView.setViewport(sf::FloatRect(
                boardLeft / WIN_W,
                boardTop  / WIN_H,
                boardW    / WIN_W,
                boardH    / WIN_H));
            window.setView(clipView);

            for (int i = 1; i <= GRID_SIZE; i++) {
                for (int j = 1; j <= GRID_SIZE; j++) {
                    const Piece& p = grid[i][j];
                    if (p.kind < 0) continue;
                    if (p.special == SPECIAL_HORIZONTAL) {
                        gems.setTexture(horizontalSpecialTex);
                        gems.setTextureRect(sf::IntRect(p.kind * HORIZ_FRAME_WIDTH, 0, HORIZ_FRAME_WIDTH, HORIZ_FRAME_HEIGHT));
                        float scale = 0.8f * 64.f / static_cast<float>(HORIZ_FRAME_WIDTH);
                        gems.setScale(scale, scale);
                    } else if (p.special == SPECIAL_VERTICAL) {
                        gems.setTexture(verticalSpecialTex);
                        gems.setTextureRect(sf::IntRect(p.kind * VERT_FRAME_WIDTH, 0, VERT_FRAME_WIDTH, VERT_FRAME_HEIGHT));
                        float scale = 0.8f * 64.f / static_cast<float>(VERT_FRAME_WIDTH);
                        gems.setScale(scale, scale);
                    } else if (p.special == SPECIAL_SQUARE) {
                        gems.setTexture(squareSpecialTex);
                        gems.setTextureRect(sf::IntRect(p.kind * SQUARE_FRAME_WIDTH, 0, SQUARE_FRAME_WIDTH, SQUARE_FRAME_HEIGHT));
                        float scale = 0.8f * 64.f / static_cast<float>(SQUARE_FRAME_WIDTH);
                        gems.setScale(scale, scale);
                    } else if (p.special == SPECIAL_COLOR) {
                        gems.setTexture(colorSpecialTex);
                        gems.setTextureRect(sf::IntRect(p.kind * COLOR_FRAME_WIDTH, 0, COLOR_FRAME_WIDTH, COLOR_FRAME_HEIGHT));
                        float scale = 0.8f * 64.f / static_cast<float>(COLOR_FRAME_WIDTH);
                        gems.setScale(scale, scale);
                    } else {
                        gems.setTexture(gemsTex);
                        gems.setTextureRect(sf::IntRect(p.kind * 64, 0, 64, 64));
                        gems.setScale(0.8f, 0.8f);
                    }
                    gems.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(p.alpha)));
                    gems.setPosition(
                        static_cast<float>(p.x + OFFSET.x - TILE_SIZE),
                        static_cast<float>(p.y + OFFSET.y - TILE_SIZE));
                    window.draw(gems);
                }
            }

            window.setView(window.getDefaultView());
        }

        if (!fontLoaded) {
            window.setTitle("Match-3 Game - Score: " + fmtScore(totalScore));
        }

        // Score panel
        if (fontLoaded) {
            if (currentTarget < 0) {
                levelPassed = false;
                gameOver = false;
            } else {
                levelPassed = totalScore >= currentTarget;
                if (levelPassed) levels[currentLevel].completed = true;
                gameOver = (levels[currentLevel].moves >= 0 && movesLeft == 0 && !levelPassed);
            }

            window.draw(scorePanel);

            window.draw(labelScore);
            valueScore.setString(fmtScore(totalScore));
            window.draw(valueScore);

            if (currentTarget < 0) {
                labelTarget.setString("MODE");
                labelTarget.setPosition(720.f, 110.f);
                window.draw(labelTarget);
                valueTarget.setString("FREE PLAY");
                valueTarget.setPosition(720.f, 136.f);
                window.draw(valueTarget);
                statusText.setString("Free play mode");
                statusText.setFillColor(sf::Color(220, 220, 220));
            } else {
                labelTarget.setString("TARGET");
                labelTarget.setPosition(720.f, 110.f);
                window.draw(labelTarget);
                valueTarget.setString(fmtScore(currentTarget));
                valueTarget.setPosition(720.f, 136.f);
                valueTarget.setFillColor(levelPassed ? sf::Color(140, 255, 140) : sf::Color(255, 200, 120));
                window.draw(valueTarget);

                if (levelPassed) {
                    statusText.setString("LEVEL COMPLETE!");
                    statusText.setFillColor(sf::Color(160, 255, 160));
                } else if (gameOver) {
                    statusText.setString("OUT OF MOVES");
                    statusText.setFillColor(sf::Color(255, 130, 130));
                } else {
                    statusText.setString("Reach the target to win");
                    statusText.setFillColor(sf::Color(220, 220, 220));
                }
            }
            window.draw(statusText);

            window.draw(labelCombo);
            if (combo > 1) {
                valueCombo.setString("x" + std::to_string(combo));
                valueCombo.setFillColor(sf::Color(255, 120, 60));
            } else {
                valueCombo.setString("-");
                valueCombo.setFillColor(sf::Color(140, 140, 140));
            }
            window.draw(valueCombo);

            if (levels[currentLevel].moves >= 0) {
                window.draw(labelMoves);
                valueMoves.setString(std::to_string(movesLeft));
                window.draw(valueMoves);
            }

            // Floating score popups
            for (auto& p : popups)
                if (p.active) window.draw(p.text);

            if (inPauseMenu) {
                window.draw(pauseOverlay);
                window.draw(pauseTitle);
                for (size_t i = 0; i < pauseOptionText.size(); ++i) {
                    pauseOptionText[i].setFillColor(i == static_cast<size_t>(pauseSelection) ? sf::Color(255,255,160) : sf::Color(220,220,255));
                    pauseOptionText[i].setPosition((1024.f - pauseOptionText[i].getGlobalBounds().width) / 2.f, 260.f + i * 50.f);
                    window.draw(pauseOptionText[i]);
                }
            }
        }

        window.display();
    }

    return 0;
}