#include <ctime>
#include <chrono>
#include <future>
#include <thread>
#include "Rendering.h"
#include <GLFW/glfw3.h>

#include <irrKlang.h>
using namespace irrklang;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Renderer* renderer = nullptr;
glm::mat4 projectionMatrix;

ISoundEngine* soundEngine = nullptr;
ISoundSource* parkingSound = nullptr;
ISoundSource* leavingSound = nullptr;
ISoundSource* indicatorSound = nullptr;

// Global variables for texture IDs
GLuint carTexture;
GLuint parkingSpotTexture;
GLuint backgroundTexture;

// Time tracking
float currentTime = 0.0f, lastTime = 0.0f;
const int TARGET_FPS = 60;
const double FRAME_DURATION_MS = 1000.0 / TARGET_FPS;

int WIDTH = 1400;
int HEIGHT = 800;

const int ROWS = 2;
const int COLUMNS = 3;
float CELL_WIDTH = WIDTH / 5.5f;
float CELL_HEIGHT = CELL_WIDTH * 1.4;

float parkingSpotDistance = 60.0f;
float additionalHorizontalSpacing = 100.0f;

bool keys[1024] = { false };

struct ParkingSpot {
    bool occupied = false;
    bool blinking = false;
    float blinkColor[3] = { 1.0f, 0.0f, 1.0f };
    float blinkTimer = 0.0f;
    float timer = 0.0f;
    bool timerSound = true;
    float redProgress = 0.0f;
    float carColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string driverName = "";
    std::string licensePlate = "";
    bool showInfo = false;
};

std::vector<ParkingSpot> parkingSpots(ROWS * COLUMNS);

// Global variables for title animation
bool displayParking = true;
float titleTextColor[3] = { 1.0f, 1.0f, 1.0f };
float targetTitleTextColor[3] = { 1.0f, 0.0f, 0.0f };
float titleTextTransitionProgress = 0.0f;
const float titleTransitionDuration = 3.0f;
bool reverseTransition = false;

// Function to load a texture from file
GLuint loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Failed to load texture: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// Call this function during initialization
void initializeTextures() {
    carTexture = loadTexture("car.png");
    parkingSpotTexture = loadTexture("parking_spot.png");
	backgroundTexture = loadTexture("background_whole.jpg");
}

std::future<void> soundLoadingFuture;
void initializeSound() {
    parkingSound = soundEngine->addSoundSourceFromFile("car_enter_parking.wav", ESM_AUTO_DETECT, true);
    leavingSound = soundEngine->addSoundSourceFromFile("car_drive_off.wav", ESM_AUTO_DETECT, true);
    indicatorSound = soundEngine->addSoundSourceFromFile("indicator_sound.wav", ESM_AUTO_DETECT, true);
    if (!parkingSound || !leavingSound|| !indicatorSound) {
        std::cerr << "Failed to load sound!" << std::endl;
    }
    soundLoadingFuture = std::async(std::launch::async, []() {
        // Play the sound once with zero volume to preload it
        soundEngine->play2D(indicatorSound, false, false, true)->setVolume(0.0f);
    });
}

// Resize callback
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    WIDTH = width;
    HEIGHT = height;
    glViewport(0, 0, width, height);

    // Update projection matrix when window is resized
    glm::mat4 newProjectionMatrix = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);

    // Update projection matrix in renderer
    renderer->setProjectionMatrix(newProjectionMatrix);
}

std::string generateLicensePlate() {
    std::string licensePlate = "";
    for (int i = 0; i < 2; ++i) {
        licensePlate += static_cast<char>(rand() % 26 + 'A');
    }
    licensePlate += " ";
    for (int i = 0; i < 3; ++i) {
        licensePlate += std::to_string(rand() % 10);
    }
    licensePlate += "-";
    for (int i = 0; i < 2; ++i) {
        licensePlate += static_cast<char>(rand() % 26 + 'A');
    }
    return licensePlate;
}

std::string generateDriverName() {
    std::vector<std::string> names = { "John", "Jane", "Alice", "Bob",
        "Charlie", "David", "Eve", "Frank", "Grace", "Hank", "Jack", "Kate" };
    std::vector<std::string> surnames = { "Smith", "Johnson", "Williams", "Jones",
        "Brown", "Davis", "Miller", "Wilson", "Moore", "Taylor", "Anderson", "Thomas",
        "Jackson", "White", "Martin", "Thompson", "Garcia", "Martinez", "Robinson",
        "Clark", "Rodriguez", "Lewis", "Lee", "Walker", "Hall", "Allen" };
    return names[rand() % names.size()] + " " + surnames[rand() % surnames.size()];
}

void handleParkingSpotEvent(int row, int col, int mods) {
    int index = row * COLUMNS + col;
    ParkingSpot& spot = parkingSpots[index];

    if (!spot.occupied && mods != GLFW_MOD_CONTROL) {
        spot.occupied = true;
        spot.timer = 20.0f;
        spot.timerSound = true;
        spot.redProgress = 0.0f;
        spot.licensePlate = generateLicensePlate();
        spot.driverName = generateDriverName();

        spot.carColor[0] = static_cast<float>(rand()) / RAND_MAX;
        spot.carColor[1] = static_cast<float>(rand()) / RAND_MAX;
        spot.carColor[2] = static_cast<float>(rand()) / RAND_MAX;

        soundEngine->play2D(parkingSound);
    }
    else if (spot.occupied && mods == GLFW_MOD_SHIFT) {
        spot.timer = 20.0f;
        spot.redProgress = 0.0f;
    }
    else if (spot.occupied && mods == GLFW_MOD_CONTROL) {
        spot.occupied = false;
        spot.timer = 0.0f;
        spot.timerSound = true;
        spot.redProgress = 0.0f;
        spot.blinking = false;
        spot.licensePlate = "";
		spot.showInfo = false;

        soundEngine->play2D(leavingSound);
    }
}

// Input handling
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            keys[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            keys[key] = false;
        }
    }

    int row = -1;
    int col = -1;

    if (keys[GLFW_KEY_A]) row = 0;
	else if (keys[GLFW_KEY_B]) row = 1;

    if (keys[GLFW_KEY_1]) col = 0;
    else if (keys[GLFW_KEY_2]) col = 1;
    else if (keys[GLFW_KEY_3]) col = 2;

    if (row != -1 && col != -1) {
        handleParkingSpotEvent(row, col, mods);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Convert y position to match OpenGL coordinate system
        ypos = HEIGHT - ypos;

        // Calculate the total width of the parking area including the additional spacing
        float totalParkingWidth = COLUMNS * (CELL_WIDTH + additionalHorizontalSpacing) - additionalHorizontalSpacing;
        float totalParkingHeight = ROWS * CELL_HEIGHT;

        float horizontalOffset = (WIDTH - totalParkingWidth) / 2.0f;
        float verticalOffset = (HEIGHT - totalParkingHeight) / 2.0f;

        for (int row = 0; row < ROWS; ++row) {
            for (int col = 0; col < COLUMNS; ++col) {
                int index = row * COLUMNS + col;
                ParkingSpot& spot = parkingSpots[index];

                float x = col * (CELL_WIDTH + additionalHorizontalSpacing) + horizontalOffset + parkingSpotDistance / 2;
                float y = (ROWS - 1 - row) * CELL_HEIGHT + verticalOffset;

                if (HEIGHT < 750) {
                    y -= (745 - HEIGHT) / 2;
                }

                float indicatorX = x - parkingSpotDistance + 10.0f;
                float indicatorY = y + CELL_HEIGHT / 2 - 35.0f;
                float radius = 37.0f;

                // Check if the mouse click is within the indicator circle
                if (spot.blinking && (pow(xpos - indicatorX, 2) + pow(ypos - indicatorY, 2) <= pow(radius, 2))) {
                    spot.occupied = false;
                    spot.blinking = false;
                    spot.timer = 0.0f;
                    spot.timerSound = true;
                    spot.redProgress = 0.0f;
                    spot.licensePlate = "";
                    spot.showInfo = false;

                    soundEngine->play2D(leavingSound);
                }

                // Check if the mouse clicked on the car
                else if (spot.occupied && (xpos >= x + 20.0f && xpos <= x + (CELL_WIDTH - parkingSpotDistance) - 40.0f) &&
                        (ypos >= y + 20.0f && ypos <= y + (CELL_HEIGHT - parkingSpotDistance) - 40.0f)) {
                    spot.showInfo = !spot.showInfo;
                }
            }
        }
    }
}

// Rendering
void render() {
    glClear(GL_COLOR_BUFFER_BIT);

	// Draw the background
    renderer->renderImage(backgroundTexture, 0.0f, 0.0f, WIDTH, HEIGHT, 0.0f, 1.0f, {1.0f, 1.0f, 1.0f});

    // Calculate the total width of the parking area including the additional spacing
    float totalParkingWidth = COLUMNS * (CELL_WIDTH + additionalHorizontalSpacing) - additionalHorizontalSpacing;
    float totalParkingHeight = ROWS * CELL_HEIGHT;

    float horizontalOffset = (WIDTH - totalParkingWidth) / 2.0f;
    float verticalOffset = (HEIGHT - totalParkingHeight) / 2.0f;

    // Draw parking spots
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            int index = row * COLUMNS + col;
            ParkingSpot& spot = parkingSpots[index];

            float x = col * (CELL_WIDTH + additionalHorizontalSpacing) + horizontalOffset + parkingSpotDistance / 2;
			float y = (ROWS - 1 - row) * CELL_HEIGHT + verticalOffset;

            if (HEIGHT < 750) {
                y -= (745 - HEIGHT) / 2;
            }

            glm::vec4 textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            float rotation = 0.0f;
            if (row == 1) {
				rotation = 180.0f;
            }

            // Draw the parking space
            renderer->renderImage(parkingSpotTexture, x, y, CELL_WIDTH - parkingSpotDistance, CELL_HEIGHT - parkingSpotDistance, rotation, 1.0f, { 1.0f, 1.0f, 1.0f });

            glm::vec3 blendColor = glm::vec3(spot.carColor[0], spot.carColor[1], spot.carColor[2]);
            // Draw the car if the spot is occupied
            if (spot.occupied) {
				// Draw the car information
                if (spot.showInfo) {
                    //renderer->drawRectangle(x + 20.0f, y + 20.0f, (CELL_WIDTH - parkingSpotDistance) - 40.0f, (CELL_HEIGHT - parkingSpotDistance) - 40.0f, semiTransparentColor);
                    renderer->renderImage(carTexture, x + 20.0f, y + 20.0f, (CELL_WIDTH - parkingSpotDistance) - 40.0f, (CELL_HEIGHT - parkingSpotDistance) - 40.0f, rotation, 0.6f, blendColor);

                    float licensePlateWidth = renderer->measureTextWidth(spot.licensePlate.c_str(), 0.5f);
                    float driverNameWidth = renderer->measureTextWidth(spot.driverName.c_str(), 0.5f);
                    float maxWidth = std::max(licensePlateWidth, driverNameWidth);
                    float blackColor[4] = { 0.0f, 0.0f, 0.0f, 0.4f };
                    float labelBoxXCoord = x + 20.0f + ((CELL_WIDTH - parkingSpotDistance) - 40.0f) / 2 - ((maxWidth + 10.0f) / 2);
                    renderer->drawRectangle(labelBoxXCoord, y + 30.0f, maxWidth + 10.0f, 52.0f, blackColor);
					
                    renderer->drawText(spot.licensePlate.c_str(), labelBoxXCoord + 5.0f, y + 35.0f, 0.5f, textColor);
                    renderer->drawText(spot.driverName.c_str(), labelBoxXCoord + 5.0f, y + 60.0f, 0.5f, textColor);
                }
                else {
                    //renderer->drawRectangle(x + 20.0f, y + 20.0f, (CELL_WIDTH - parkingSpotDistance) - 40.0f, (CELL_HEIGHT - parkingSpotDistance) - 40.0f, spot.carColor);
					renderer->renderImage(carTexture, x + 20.0f, y + 20.0f, (CELL_WIDTH - parkingSpotDistance) - 40.0f, (CELL_HEIGHT - parkingSpotDistance) - 40.0f, rotation, 1.0f, blendColor);
                }
            }

            // Draw the spot indicator
			float indicatorBorderColor[3] = { 1.0f, 1.0f, 1.0f };
            renderer->drawCircle(x - (parkingSpotDistance) + 10.0f, y + CELL_HEIGHT / 2 - 35.0f, 37.0f, indicatorBorderColor);
            if (spot.blinking) {
                renderer->drawCircle(x - (parkingSpotDistance) + 10.0f, y + CELL_HEIGHT / 2 - 35.0f, 35.0f, spot.blinkColor);
                if (spot.timerSound) {
                    soundEngine->play2D(indicatorSound);
                    spot.timerSound = false;
                }
            }
            else {
                renderer->drawParkingSpotTimer(x - (parkingSpotDistance) + 10.0f, y + CELL_HEIGHT / 2 - 35.0f, 35.0f, spot.redProgress);
            }

            // Draw the parking spot label
            std::string label = (row == 0 ? "A" : "B") + std::to_string(col + 1);
			float labelWidth = renderer->measureTextWidth(label.c_str(), 0.5f);
            renderer->drawText(label.c_str(), x + (CELL_WIDTH - parkingSpotDistance) - labelWidth, y - 23.0f, 0.5f, textColor);
        }
    }

    // Draw the title
    float titleWidth = renderer->measureTextWidth("PARKING", 1.0f);
    float blackColor[4] = { 0.0f, 0.0f, 0.0f, 0.4f };
    renderer->drawRectangle(WIDTH / 2 - (titleWidth / 2) - 5.0f, HEIGHT - 65.0f, titleWidth + 10.0f, 48.0f, blackColor);

    std::string message = displayParking ? "PARKING" : "SERVIS";
    float alpha1 = displayParking ? (1.0f - titleTextTransitionProgress) : titleTextTransitionProgress;
    float alpha2 = 1.0f - alpha1;

    if (!displayParking) {
		glm::vec4 titleTextColorVec = glm::vec4(titleTextColor[0], titleTextColor[1], titleTextColor[2], alpha1);
		std::string serviceText = "SERVIS";
		std::string parkingText = "PARKING";
		float widthDiff = renderer->measureTextWidth(parkingText.c_str(), 1.0f) - renderer->measureTextWidth(serviceText.c_str(), 1.0f);
        renderer->drawText(message.c_str(), WIDTH / 2 - (titleWidth / 2) + (widthDiff / 2), HEIGHT - 58.0f, 1.0f, titleTextColorVec);
    }
    else {
		glm::vec4 titleTextColorVec = glm::vec4(titleTextColor[0], titleTextColor[1], titleTextColor[2], alpha2);
        renderer->drawText(message.c_str(), WIDTH / 2 - (titleWidth / 2), HEIGHT - 58.0f, 1.0f, titleTextColorVec);
    }

    std::string additionalText = "Vuk Dimitrov SV52/2021";
    float additionalTextWidth = renderer->measureTextWidth(additionalText.c_str(), 0.5f);
    glm::vec4 studentNameTextColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    renderer->drawText(additionalText.c_str(), WIDTH - additionalTextWidth - 5.0f, HEIGHT - 25.0f, 0.5f, studentNameTextColor);
}

// Update logic
void update() {
    currentTime = glfwGetTime();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    // Update parking spot timers
    for (int i = 0; i < parkingSpots.size(); ++i) {
        ParkingSpot& spot = parkingSpots[i];
        if (spot.occupied) {
            spot.timer -= deltaTime;
            if (spot.timer <= 0.0f) {
                spot.timer = 0.0f;

                // Print the expired parking information
                if (spot.blinking == false) {
                    time_t now = time(0);
                    tm localTime;
                    localtime_s(&localTime, &now);
                    std::string spotName = (i < 3 ? "A" : "B") + std::to_string(i % 3 + 1);
                    std::cout << "Parking spot " << spotName << " expired at "
                        << localTime.tm_hour << ":" << localTime.tm_min << ":"
                        << localTime.tm_sec << " with vehicle: " << spot.licensePlate << std::endl;
                }
                spot.blinking = true;
            }
            spot.redProgress = 1.0f - (spot.timer / 20.0f);
        }
        else {
            spot.redProgress = 0.0f;
            spot.blinking = false;
        }

        // Update blink timer
        if (spot.blinking) {
            spot.blinkTimer += deltaTime;
            if (spot.blinkTimer >= 0.5f) {
                spot.blinkColor[2] = spot.blinkColor[2] == 1.0f ? 0.0f : 1.0f;
                spot.blinkTimer = 0.0f;
            }
        }
    }

    // Update title text animation
    if (reverseTransition) {
        titleTextTransitionProgress -= deltaTime / titleTransitionDuration;
    }
    else {
        titleTextTransitionProgress += deltaTime / titleTransitionDuration;
    }

    if (titleTextTransitionProgress >= 1.0f) {
        titleTextTransitionProgress = 1.0f;
        reverseTransition = true;
    }
    else if (titleTextTransitionProgress <= 0.0f) {
        titleTextTransitionProgress = 0.0f;
        reverseTransition = false;
        displayParking = !displayParking;
        titleTextTransitionProgress = 0.0f;
        // Set new target color
        targetTitleTextColor[0] = 0.25f + static_cast<float>(rand()) / (RAND_MAX / 0.75f);
        targetTitleTextColor[1] = 0.25f + static_cast<float>(rand()) / (RAND_MAX / 0.75f);
        targetTitleTextColor[2] = 0.25f + static_cast<float>(rand()) / (RAND_MAX / 0.75f);



    }

    // Interpolate text color
    titleTextColor[0] += (targetTitleTextColor[0] - titleTextColor[0]) * deltaTime * 2;
    titleTextColor[1] += (targetTitleTextColor[1] - titleTextColor[1]) * deltaTime * 2;
    titleTextColor[2] += (targetTitleTextColor[2] - titleTextColor[2]) * deltaTime * 2;
}

unsigned char* loadImage(const char* filename, int* width, int* height, int* channels) {
    unsigned char* image = stbi_load(filename, width, height, channels, 0);
    if (!image) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return nullptr;
    }
    return image;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Parking Servis", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewInit();

    soundEngine = createIrrKlangDevice();

    if (!soundEngine)
        return 0;

    initializeSound();

    srand(static_cast<unsigned int>(time(0)));

    // Create renderer
    renderer = new Renderer(WIDTH, HEIGHT);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    glViewport(0, 0, WIDTH, HEIGHT);

    // Set clear color
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	initializeTextures();

    // Load the cursor image
    int width, height, channels;
    unsigned char* image = loadImage("cursor.png", &width, &height, &channels);
    if (!image) {
        std::cerr << "Failed to load cursor image" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Create a GLFW image
    GLFWimage cursorImage;
    cursorImage.width = width;
    cursorImage.height = height;
    cursorImage.pixels = image;

    // Create a custom cursor
    GLFWcursor* customCursor = glfwCreateCursor(&cursorImage, 0, 0);
    if (!customCursor) {
        std::cerr << "Failed to create custom cursor" << std::endl;
        // Free the image data if necessary
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Set the custom cursor
    glfwSetCursor(window, customCursor);

    // Target frame duration for 60 fps
    const std::chrono::duration<double, std::milli> frameDuration(FRAME_DURATION_MS);
    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        update();
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();

        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = frameEnd - frameStart;

        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }

    // Cleanup
    delete renderer;

    soundEngine->drop();
    glfwDestroyCursor(customCursor);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}