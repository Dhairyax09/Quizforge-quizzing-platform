#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <windows.h> // Native Windows API
#include <conio.h>   // For _kbhit() and _getch()

using namespace std;

// --- UI Helper ---
void clearScreenAndHeader() {
    system("cls"); // Windows specific clear
    cout << "===========================================\n";
    cout << "      === QuizForge Terminal System ===    \n";
    cout << "===========================================\n\n";
}

bool getTimedInput(int seconds, char& inputChar) {
    auto start = std::chrono::steady_clock::now();
    
    // Loop until the specified time has elapsed
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < seconds) {
        if (_kbhit()) { // Check if a key has been pressed
            inputChar = _getch(); // Read the character without waiting for Enter
            cout << inputChar << "\n"; 
            return true;
        }
        Sleep(50); // Pause briefly to reduce CPU usage
    }
    return false; // Timed out
}

// --- Data Structures ---
struct Question {
    string text;
    string options[4];
    char correctOption;
};

// Abstract base class for Game Modes
class GameMode {
protected:
    int timeLimit; 
public:
    GameMode(int t) : timeLimit(t) {}
    virtual bool isTimed() const = 0;
    int getTimeLimit() const { return timeLimit; }
    virtual ~GameMode() {}
};

class TimedMode : public GameMode {
public:
    TimedMode(int t) : GameMode(t) {}
    bool isTimed() const override { return true; }
};

class NonTimedMode : public GameMode {
public:
    NonTimedMode() : GameMode(0) {}
    bool isTimed() const override { return false; }
};

// --- Class: User ---
class User {
private:
    string username;
    int highScores[6]; // 0-2: Timed (E/M/H), 3-5: Non-Timed (E/M/H)

public:
    User(string name = "") : username(name) {
        for(int i=0; i<6; i++) highScores[i] = 0;
    }

    string getUsername() const { return username; }
    int getHighScore(int index) const { return highScores[index]; }
    void setHighScore(int index, int score) { highScores[index] = score; }
    
    void loadScores(int s0, int s1, int s2, int s3, int s4, int s5) {
        highScores[0] = s0; highScores[1] = s1; highScores[2] = s2;
        highScores[3] = s3; highScores[4] = s4; highScores[5] = s5;
    }
};

// --- Class: ScoreManager ---
class ScoreManager {
private:
    const string filename = "scores.txt";

public:
    bool userExists(const string& username, User& foundUser) {
        ifstream file(filename);
        string name;
        int s[6];
        while (file >> name >> s[0] >> s[1] >> s[2] >> s[3] >> s[4] >> s[5]) {
            if (name == username) {
                foundUser = User(name);
                foundUser.loadScores(s[0], s[1], s[2], s[3], s[4], s[5]);
                file.close();
                return true;
            }
        }
        file.close();
        return false;
    }

    void saveOrUpdateUser(const User& user) {
        vector<User> users;
        ifstream inFile(filename);
        string name;
        int s[6];
        bool found = false;

        while (inFile >> name >> s[0] >> s[1] >> s[2] >> s[3] >> s[4] >> s[5]) {
            if (name == user.getUsername()) {
                users.push_back(user);
                found = true;
            } else {
                User u(name);
                u.loadScores(s[0], s[1], s[2], s[3], s[4], s[5]);
                users.push_back(u);
            }
        }
        if (!found) users.push_back(user);
        inFile.close();

        ofstream outFile(filename);
        for (const auto& u : users) {
            outFile << u.getUsername() << " ";
            for(int i=0; i<6; i++) outFile << u.getHighScore(i) << " ";
            outFile << endl;
        }
        outFile.close();
    }
};

// --- Class: Quiz ---
class Quiz {
private:
    vector<Question> questions;
    User& currentUser;
    GameMode* mode;
    string difficulty;
    int currentScore;

    int getModeIndex() {
        int base = mode->isTimed() ? 0 : 3;
        if (difficulty == "Easy") return base + 0;
        if (difficulty == "Medium") return base + 1;
        return base + 2; 
    }

    void loadQuestionsFromFile() {
        string filename = "questions_" + difficulty + ".txt";
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Error: Could not open " << filename << "\n";
            return;
        }

        Question q;
        string correctAnsStr;
        while (getline(file, q.text)) {
            if (q.text.empty()) continue;
            getline(file, q.options[0]);
            getline(file, q.options[1]);
            getline(file, q.options[2]);
            getline(file, q.options[3]);
            getline(file, correctAnsStr);
            q.correctOption = toupper(correctAnsStr[0]);
            questions.push_back(q);
        }
        file.close();

        unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
        shuffle(questions.begin(), questions.end(), std::default_random_engine(seed));
        if (questions.size() > 5) questions.resize(5);
    }

public:
    Quiz(User& user, GameMode* m, string diff) 
        : currentUser(user), mode(m), difficulty(diff), currentScore(0) {
        loadQuestionsFromFile();
    }

    void start() {
        if (questions.empty()) {
            cout << "No questions found. Press Enter to return...";
            cin.ignore(); cin.get();
            return;
        }

        for (size_t i = 0; i < questions.size(); ++i) {
            clearScreenAndHeader();
            cout << "USER: " << currentUser.getUsername() << " | MODE: " 
                 << (mode->isTimed() ? "Timed" : "Non-Timed") 
                 << " | DIFF: " << difficulty << "\n";
            cout << "SCORE: " << currentScore << "\n";
            
            if (mode->isTimed()) {
                cout << "TIME LIMIT: " << mode->getTimeLimit() << "s per question\n";
            }
            cout << "-------------------------------------------\n";
            cout << "Q" << (i + 1) << " of " << questions.size() << ": " << questions[i].text << "\n";
            cout << "a) " << questions[i].options[0] << "\n";
            cout << "b) " << questions[i].options[1] << "\n";
            cout << "c) " << questions[i].options[2] << "\n";
            cout << "d) " << questions[i].options[3] << "\n";

            char choice = 'X';
            bool answered = false;
            cout << "\nEnter Ans (a/b/c/d): ";
            
            if (mode->isTimed()) {
                answered = getTimedInput(mode->getTimeLimit(), choice);
            } else {
                cin >> choice;
                answered = true;
            }

            if (!answered) {
                cout << "\nTIME OUT! Skipped. (-1)\n";
                currentScore -= 1;
            } else {
                choice = toupper(choice);
                if (choice == questions[i].correctOption) {
                    cout << "\nCORRECT! (+4)\n";
                    currentScore += 4;
                } else {
                    cout << "\nINCORRECT! (-1)\n";
                    currentScore -= 1;
                }
            }
            
            cout << "Press Enter to continue...";
            cin.ignore(1000, '\n'); 
            cin.get();
        }
        showResults();
    }

    void showResults() {
        clearScreenAndHeader();
        int modeIdx = getModeIndex();
        int prevHigh = currentUser.getHighScore(modeIdx);

        cout << "=== QUIZ OVER ===\n";
        cout << "Final Score: " << currentScore << "\n\n";

        ScoreManager sm;
        if (currentScore > prevHigh) {
            cout << "*** NEW HIGH SCORE FOR THIS MODE! ***\n";
            currentUser.setHighScore(modeIdx, currentScore);
            sm.saveOrUpdateUser(currentUser);
        } else {
            cout << "Previous High Score for this mode: " << prevHigh << "\n";
        }

        cout << "\nPress Enter to return to menu...";
        cin.get();
    }
};

// --- Main ---
int main() {
    ScoreManager sm;
    User sessionUser;
    bool loggedIn = false;

    while (true) {
        clearScreenAndHeader();
        cout << "1. New User\n2. Existing User\n3. Exit\nSelection: ";
        string choiceStr;
        cin >> choiceStr;

        if (choiceStr == "3") break;

        string name;
        cout << "Enter Username: ";
        cin >> name;

        if (choiceStr == "1") {
            if (sm.userExists(name, sessionUser)) {
                cout << "Username already exists! Try logging in.\n";
                cout << "Press Enter...";
                cin.ignore(); cin.get();
                continue;
            }
            sessionUser = User(name);
            sm.saveOrUpdateUser(sessionUser);
            loggedIn = true;
        } else if (choiceStr == "2") {
            if (sm.userExists(name, sessionUser)) {
                loggedIn = true;
            } else {
                cout << "User not found!\n";
                cout << "Press Enter...";
                cin.ignore(); cin.get();
                continue;
            }
        }

        while (loggedIn) {
            clearScreenAndHeader();
            cout << "Logged in as: " << sessionUser.getUsername() << "\n";
            cout << "--- GAME MODE ---\n1. Timed Mode\n2. Non-Timed Mode\n3. Logout\nSelection: ";
            string mChoice;
            cin >> mChoice;

            if (mChoice == "3") { loggedIn = false; break; }
            if (mChoice != "1" && mChoice != "2") continue;

            clearScreenAndHeader();
            cout << "--- DIFFICULTY ---\n1. Easy\n2. Medium\n3. Hard\nSelection: ";
            string dChoice;
            cin >> dChoice;
            
            string diff = "Easy";
            if (dChoice == "2") diff = "Medium";
            else if (dChoice == "3") diff = "Hard";

            GameMode* mode;
            if (mChoice == "1") {
                int t = (diff == "Easy") ? 15 : 10;
                mode = new TimedMode(t);
            } else {
                mode = new NonTimedMode();
            }

            Quiz quiz(sessionUser, mode, diff);
            quiz.start();
            delete mode;
        }
    }
    return 0;
}
