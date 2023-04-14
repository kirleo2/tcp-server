#include <iostream>
using namespace std;


#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h> // bzero()
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_set>
#include <math.h>
//#include <wait.h> // waitpid()

#define BUFFER_SIZE 10240
#define TIMEOUT_MESSAGE 1
#define TIMEOUT_CHARGING 5
#define MAX_LENGTH 100
#define SERVER_KEY 0
#define CLIENT_KEY 1


enum cleint_messages : int{
    CLIENT_USERNAME=0,
    CLIENT_KEY_ID=1,
    CLIENT_CONFIRMATION=2,
    CLIENT_OK=3,
    CLIENT_RECHARGING=4,
    CLIENT_FULL_POWER=5,
    CLIENT_MESSAGE=6
};
enum server_messages : int {
    SERVER_CONFIRMATION=7,
    SERVER_MOVE=8,
    SERVER_TURN_LEFT=9,
    SERVER_TURN_RIGHT=10,
    SERVER_PICK_UP=11,
    SERVER_LOGOUT=12,
    SERVER_KEY_REQUEST=13,
    SERVER_OK=14,
    SERVER_LOGIN_FAILED=15,
    SERVER_SYNTAX_ERROR=16,
    SERVER_LOGIC_ERROR=17,
    SERVER_KEY_OUT_OF_RANGE_ERROR=18
};

enum max_lengths : size_t {
    CLIENT_USERNAME_MAX_LENGTH = 18,
    CLIENT_KEY_ID_MAX_LENGTH = 3,
    CLIENT_CONFIRMATION_MAX_LENGTH = 5,
    CLIENT_OK_MAX_LENGTH = 10,
    CLIENT_RECHARGING_MAX_LENGTH = 10,
    CLIENT_FULL_POWER_MAX_LENGTH = 10,
    CLIENT_MESSAGE_MAX_LENGTH = 98
};
enum directions : int {
    LEFT = 0,
    STRAIGHT = 1,
    RIGHT = 2,
    BACK = 3,
    UNDEFINED_DIRECTION = 4
};


static const string CLIENT_RECHARGING_TEXT = "RECHARGING";
static const string CLIENT_FULL_POWER_TEXT = "FULL POWER";


struct incompleteMessage {
    string buffer;
    string waitingFor;
    bool isClear = true;
    void clear() {
        waitingFor = "";
        buffer = "";
        isClear = true;
    }
};

static const vector< vector<uint16_t> > KEY_PAIRS = {
    vector<uint16_t>{23019, 32037},
    vector<uint16_t>{32037, 29295},
    vector<uint16_t>{18789, 13603},
    vector<uint16_t>{16443, 29533},
    vector<uint16_t>{18189, 21952}
};


class ServerError : public exception {
public:
    
    ServerError(int errorID) : errorID(errorID){}
    int errorId () {
        return errorID;
    }
private:
    int errorID;
    
};

class Robot {
    
public:
    int x_pos;
    int y_pos;
    int direction;
    
    Robot(){
        
        username = "";
        waitingFor = CLIENT_USERNAME;
        x_pos = 0;
        y_pos = 0;
        keyID = -1;
        lastMove = -1;
        direction = UNDEFINED_DIRECTION;
        prevState = CLIENT_USERNAME;
        firstMove = true;
        
    }
    int getLastMove() const{
        return lastMove;
    }
    int getPrevState() const {
        return prevState;
    }
    string getIncompleteMessage() {
        return incMsg.buffer;
    }
    
    bool isFirstMove() {
        bool tmp = firstMove;
        firstMove = false;
        return tmp;
    }
    
    void setCurrentState(int state) {
        prevState = waitingFor;
        waitingFor = state;
    }
    void setKeyID(int key) {
        keyID = key;
    }
    void setUsername(const string & uname) {
        username = uname;
    }
    const int isWaitingFor() const{
        return waitingFor;
    }
    
  
    
    int getBestMove() {
        if (queueOfmoves.size()) {
            return queueOfmoves.front();
        }
            
        auto positions = getPossiblePositions();
        pair<int, int> bestPos;
        double distance = INFINITY;
        for (auto position : positions) {
            if (forbiddenCoords.find(position) == forbiddenCoords.end()) {
                if (getDistanceFromTarget(position) < distance) {
                    cout << getDistanceFromTarget(position) << " je mensi nez " << distance << endl;
                    distance = getDistanceFromTarget(position);
                    bestPos = position;
                }
            }
        }
        cout << bestPos.first << " - " << bestPos.second << endl;
        if (getPositionAfterMove(direction) == bestPos) {
            queueOfmoves.push_back(SERVER_MOVE);
        }
        else {
            int future_direction = direction;
            while (getPositionAfterMove(future_direction) != bestPos) {
                future_direction = (future_direction + 1) % 4;
                queueOfmoves.push_back(SERVER_TURN_RIGHT);
            }
        }
        return queueOfmoves.front();
    }
    

    
    void popMove() {
        queueOfmoves.pop_front();
    }
    void forbidPosition() {
        forbiddenCoords.insert({x_pos, y_pos});
    }
    void addBarrier() {
        forbiddenCoords.insert(getPositionAfterMove(direction));
    }
    
    bool clientKeyConfirmationPassed (int key) const {
        uint16_t clKey = (uint16_t) key;
        
        clKey -= KEY_PAIRS[keyID][CLIENT_KEY];
        
        if (clKey == getUsernameHash()) return true;
        return false;
    }
    string makeMessage(int msgId) {
        if (msgId == SERVER_SYNTAX_ERROR) {
            return "301 SYNTAX ERROR\a\b";
        }
        else if (msgId == SERVER_KEY_REQUEST) {
            return "107 KEY REQUEST\a\b";
        }
        else if (msgId == SERVER_KEY_OUT_OF_RANGE_ERROR) {
            return "303 KEY OUT OF RANGE\a\b";
        }
        else if (msgId == SERVER_LOGIC_ERROR) {
            return "302 LOGIC ERROR\a\b";
        }
        else if (msgId == SERVER_LOGIN_FAILED) {
            return "300 LOGIN FAILED\a\b";
        }
        else if (msgId == SERVER_OK) {
            return "200 OK\a\b";
        }
        else if (msgId == SERVER_LOGOUT) {
            return "106 LOGOUT\a\b";
        }
        else if (msgId == SERVER_PICK_UP) {
            return "105 GET MESSAGE\a\b";
        }
        else if (msgId == SERVER_TURN_RIGHT) {
            lastMove = SERVER_TURN_RIGHT;
            
            return "104 TURN RIGHT\a\b";
        }
        else if (msgId == SERVER_TURN_LEFT) {
            lastMove = SERVER_TURN_LEFT;
            return "103 TURN LEFT\a\b";
        }
        else if (msgId == SERVER_MOVE) {
            lastMove = SERVER_MOVE;
            return "102 MOVE\a\b";
        }
        else if (msgId == SERVER_CONFIRMATION) {
            uint16_t hash = getUsernameHash();
            hash += KEY_PAIRS[keyID][SERVER_KEY];
            return to_string(hash) + "\a\b";
        }
        
        return "";
    }
    
    int processBuffer(vector<char> & buffer, deque<string> & queryQueue, int bytesRead) {
        string buf = buffer.data();
        if (buf.length() != bytesRead) {
            // ve buffferu vyskytla ukoncujici nula
            buf = "";
            for (int i = 0; i < bytesRead; i++) buf += buffer[i] + ""s;
        }
        cout << "DATA: " << buf << endl;
        while (buf.length()) {
            size_t pos;
            if (!incMsg.isClear) {
                if (incMsg.waitingFor == "\b" && buf[0] == '\b') {
                    buf.erase(0, 1);
                   
                    queryQueue.push_back(incMsg.buffer);
                    
                    incMsg.clear();
                    continue;
                }
                else {
                    incMsg.waitingFor = "\a\b";
                    pos = buf.find(incMsg.waitingFor);
                    if (pos == string::npos) {
                        if (buf.back() == '\a') {
                            incMsg.waitingFor = "\b";
                            incMsg.buffer += buf.substr(0, buf.length() - 1);
                        }
                        else {
                            incMsg.buffer += buf;
                        }
                    }
                    else {
                        string toQueue;
                        toQueue = incMsg.buffer + buf.substr(0, pos);


                        queryQueue.push_back(toQueue);

                        incMsg.clear();
                   
                    }
                    
                }
   
            }
            else {
                pos = buf.find("\a\b");
                if (pos != string::npos) {
                    string toQueue;
                    toQueue = buf.substr(0, pos);
                    
                    cout << "toPush : " << toQueue << endl;
                    queryQueue.push_back(toQueue);
                }
                else {
                    if (buf.back() == '\a') {
                        incMsg.waitingFor = "\b";
                        incMsg.buffer = buf.substr(0, buf.length() - 1);
                 
                    }
                    else {
                        incMsg.waitingFor = "\a\b";
                        incMsg.buffer = buf;
                        
                    }
                    incMsg.isClear = false;
                    
                }
            }
            buf.erase(0, pos != string::npos ? pos + 2 : buf.length());
            
        }
        return 0;
    }
    

private:
    struct hash_pair {
        template <class T1, class T2>
        size_t operator()(const pair<T1, T2>& p) const
        {
            auto hash1 = hash<T1>{}(p.first);
            auto hash2 = hash<T2>{}(p.second);
     
            if (hash1 != hash2) {
                return hash1 ^ hash2;
            }
             
              return hash1;
        }
        
    };
private:
    
    deque< int > queueOfmoves;
    int prevState;
    int waitingFor;
    incompleteMessage incMsg;
    string username;
    int keyID;
    bool firstMove;
    int lastMove;
    unordered_set < pair<int, int>, hash_pair > forbiddenCoords;
    

private:
    uint16_t getUsernameHash() const{
        uint16_t hash = 0;
        for (char ch : username) {
            hash += ch;
        }
        hash *= 1000;
        cout << "hash :" << hash << endl;
        return hash;
    }
    double getDistanceFromTarget (pair<int, int> pos) const{
        
        double dist = sqrt(pow(abs(pos.first), 2) + pow(abs(pos.second), 2));
        cout << "Distance " <<pos.first << " - " << pos.second <<  " se rovna " << dist << endl;
        return dist;
    }
    pair<int, int> getPositionAfterMove(int direction) {
        if (direction == STRAIGHT) return {x_pos, y_pos + 1};
        if (direction == BACK) return {x_pos, y_pos - 1};
        if (direction == RIGHT) return {x_pos + 1, y_pos};
        if (direction == LEFT) return {x_pos - 1, y_pos};
        return {x_pos, y_pos};
    }
    vector< pair<int, int> > getPossiblePositions() {
        pair<int, int> pos1 = {x_pos, y_pos + 1};
        pair<int, int> pos2 = {x_pos, y_pos - 1};
        pair<int, int> pos3 = {x_pos + 1, y_pos};
        pair<int, int> pos4 = {x_pos - 1, y_pos};
        vector<pair<int, int>> positions;
        positions.push_back(pos1);
        positions.push_back(pos2);
        positions.push_back(pos3);
        positions.push_back(pos4);
        return positions;
    }
    
};

int main(int argc, char **argv) {
    

    if(argc < 2){
        cerr << "Usage: server port" << endl;
        return -1;
    }

    // Vytvoreni koncoveho bodu spojeni
    int l = socket(AF_INET, SOCK_STREAM, 0);
    if(l < 0){
        perror("Nemohu vytvorit socket: ");
        return -1;
    }

    int port = atoi(argv[1]);
    if(port == 0){
        cerr << "Usage: server port" << endl;
        close(l);
        return -1;
    }

    struct sockaddr_in adresa;
    bzero(&adresa, sizeof(adresa));
    adresa.sin_family = AF_INET;
    adresa.sin_port = htons(port);
    adresa.sin_addr.s_addr = htonl(INADDR_ANY);

    // Prirazeni socketu k rozhranim

    if(::bind(l, (struct sockaddr *) &adresa, sizeof(adresa)) < 0){
        perror("Problem s bind(): ");
        close(l);
        return -1;
    }

    // Oznacim socket jako pasivni
    if(listen(l, 10) < 0){
        perror("Problem s listen()!");
        close(l);
        return -1;
    }

    struct sockaddr_in vzdalena_adresa;
    socklen_t velikost;

    while(true){
        // Cekam na prichozi spojeni
        int c = accept(l, (struct sockaddr *) &vzdalena_adresa, &velikost);
        if(c < 0){
            perror("Problem s accept()!");
            close(l);
            break;
        }
        pid_t pid = fork();
        if(pid == 0){
            Robot robot;
            deque<string> queryQueue;

            // Kopie hlavniho vlakna ma vlastni referenci na naslouchajici soket.
            // Podproces, ktery obsluhuje klienta, tuto referenci nepotrebuje, takze je dobre
            // tuto referenci smazat. V hlavnim vlakne samozrejme reference na naslouchajici
            // soket zustava.
            close(l);

            struct timeval timeout;
            timeout.tv_sec = TIMEOUT_MESSAGE;
            timeout.tv_usec = 0;

            fd_set sockets;

            int retval;

            while(true){
                FD_ZERO(&sockets);
                FD_SET(c, &sockets);
                // Prvnim parametrem je cislo nejvyssiho soketu v 'sockets' zvysene o jedna.
                // (Velka jednoduchost a efektivvnost funkce je vyvazena spatnou
                // ergonomii pro uzivatele.)
                // Posledni z parametru je samotny timeout. Je to struktura typu 'struct timeval',
                // ktera umoznuje casovani s presnosti na mikrosekundy (+/-). Funkci se predava
                // odkaz na promennou a funkce ji muze modifikovat. Ve vetsine implementaci
                // odecita funkce od hodnoty timeoutu cas straveny cekanim. Podle manualu na
                // to nelze spolehat na vsech platformach a ve vsech implementacich funkce
                // select()!!!
                retval = select(c + 1, &sockets, NULL, NULL, &timeout);


                if(retval < 0){
                    perror("Chyba v select(): ");
                    close(c);
                    break;
                }
                if(!FD_ISSET(c, &sockets)){
                    // Zde je jasne, ze funkce select() skoncila cekani kvuli timeoutu.
                    cout << "Connection timeout!" << endl;
                    close(c);
                    break;
                }
                vector<char> buffer(BUFFER_SIZE);
                long int bytesRead = recv(c, buffer.data(), BUFFER_SIZE - 1, 0);
                cout << "precteno bajtu :" << bytesRead << endl;
                
                

                if(bytesRead <= 0){

                    perror("Chyba pri cteni ze socketu: ");
                    close(c);
                    break;
                }


                timeout.tv_sec = TIMEOUT_MESSAGE;
                timeout.tv_usec = 0;

                buffer[bytesRead] = '\0';
                cout << buffer.data() << endl;
                
                robot.processBuffer(buffer, queryQueue, (int) bytesRead);
                bool closeConnection = false;
                string reply;
                while (queryQueue.size()) {
                    try {
                        string message = queryQueue.front();
                        queryQueue.pop_front();
                        if (message == CLIENT_RECHARGING_TEXT) {
                            robot.setCurrentState(CLIENT_FULL_POWER);
                            timeout.tv_sec = TIMEOUT_CHARGING;
                            timeout.tv_usec = 0;
                            continue;
                        }
                        if (message == CLIENT_FULL_POWER_TEXT && robot.isWaitingFor() != CLIENT_FULL_POWER) {
                            throw ServerError(SERVER_LOGIC_ERROR);
                        }
                        
                        
                        if (robot.isWaitingFor() == CLIENT_USERNAME) {
                            if (message.length() > CLIENT_USERNAME_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                            reply = robot.makeMessage(SERVER_KEY_REQUEST);
                            robot.setCurrentState(CLIENT_KEY_ID);
                            robot.setUsername(message);


                            long int sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                            if(sent < 0){

                                closeConnection = true;
                                break;
                            }
                            cout << "pozadavek o keyID byl odeslan" << endl;

                            // send reply
                        }
                        else if (robot.isWaitingFor() == CLIENT_KEY_ID) {
                            if (message.length() > CLIENT_KEY_ID_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                            cout << "KEY_ID FROM CLIENT : "<< message << endl;;
                            try {
                                int keyID = stoi(message);
                                
                                if (keyID > 4 || keyID < 0) {
                                    throw ServerError(SERVER_KEY_OUT_OF_RANGE_ERROR);
                                }
                                robot.setKeyID(keyID);
                                reply = robot.makeMessage(SERVER_CONFIRMATION);
                                robot.setCurrentState(CLIENT_CONFIRMATION);
                                // send reply
                                long int sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);
                                if(sent < 0){
                                    closeConnection = true;
                                    break;
                                }


                            } catch (invalid_argument & e) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }

                        }
                        else if (robot.isWaitingFor() == CLIENT_CONFIRMATION) {
                            if (message.length() > CLIENT_CONFIRMATION_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                            try {
                                for (char ch : message) if (!isdigit(ch)) throw ServerError(SERVER_SYNTAX_ERROR);
                                
                                int clientKeyConfirm = stoi(message);
                                if (clientKeyConfirm < 0 || clientKeyConfirm > 65535) throw ServerError(SERVER_LOGIN_FAILED);
                                if (robot.clientKeyConfirmationPassed(clientKeyConfirm)) {

                                    reply = robot.makeMessage(SERVER_OK);
                                    //send reply
                                    // zacit se ptat na souradnice
                                    long int sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);
                                    reply = robot.makeMessage(SERVER_MOVE);
                                    sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                                    if(sent < 0) {
                                        closeConnection = true;
                                        break;
                                    }
                                    robot.setCurrentState(CLIENT_OK);
                                }
                                else {
                                    throw ServerError(SERVER_LOGIN_FAILED);
                                }

                            }
                            catch (invalid_argument & e) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }

                        }
                        else if (robot.isWaitingFor() == CLIENT_OK) {
                            if (message.length() > CLIENT_OK_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }

                            if (message.find(".") != string::npos) throw ServerError(SERVER_SYNTAX_ERROR);
                            stringstream ss;
                            ss << message;
                            int counter = 0, last_x = 0, last_y = 0;
                            while (!ss.eof()) {

                                try {
                                    string temp;

                                    ss >> temp;

                                    if (counter == 1) {
                                        int x = stoi(temp);
                                        last_x = robot.x_pos;
                                        robot.x_pos = x;

                                    }
                                    else if (counter == 2) {
                                        int y = stoi(temp);
                                        last_y = robot.y_pos;
                                        robot.y_pos = y;
                                    }

                                    counter++;

                                }
                                catch (invalid_argument & e) {

                                    throw ServerError(SERVER_SYNTAX_ERROR);
                                }
                            }
                            cout << counter << "cnt" << endl;
                            if (counter != 3) throw ServerError(SERVER_SYNTAX_ERROR);
                            if (robot.direction != UNDEFINED_DIRECTION) {
                                if (robot.getLastMove() == SERVER_TURN_RIGHT) robot.direction = (robot.direction + 1) % 4;
                                if (robot.getLastMove() == SERVER_TURN_LEFT) robot.direction = abs((robot.direction - 1 ) % 4);
                                robot.forbidPosition();
                                robot.popMove();
                            }
                            if (robot.x_pos == 0 && robot.y_pos == 0) {
                                // je na miste

                                reply = robot.makeMessage(SERVER_PICK_UP);
                                robot.setCurrentState(CLIENT_MESSAGE);
                                long int sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                                if(sent < 0) {
                                    closeConnection = true;
                                    break;
                                }

                                continue;

                            }


                            int x_change = robot.x_pos - last_x;
                            int y_change = robot.y_pos - last_y;
                            int move;
                            if (!robot.isFirstMove()) {
                                if (!x_change && !y_change && robot.getLastMove() == SERVER_MOVE) {


                                    if (robot.direction != UNDEFINED_DIRECTION){
                                        cout << "narazil na prekazku"<< endl;
                                        robot.addBarrier();
                                        move = robot.getBestMove();
                                    }
                                    else {
                                        move = SERVER_TURN_RIGHT;
                                    }

                                }
                                else if (robot.direction == UNDEFINED_DIRECTION) {
                                    if (robot.getLastMove() == SERVER_MOVE) {
                                        if (x_change) {
                                            if (x_change > 0) {
                                                robot.direction  = RIGHT;
                                            }
                                            else {
                                                robot.direction = LEFT;
                                            }
                                        }
                                        if (y_change) {
                                            if (y_change > 0) {
                                                robot.direction  = STRAIGHT;
                                            }
                                            else {
                                                robot.direction  = BACK;
                                            }
                                        }
                                        move = robot.getBestMove();

                                    }
                                    else move = SERVER_MOVE;
                                }
                                else move = robot.getBestMove();
                            }
                            else {
                                cout << "sem tady" << endl;
                                move = SERVER_MOVE;
                            }

                            reply = robot.makeMessage(move);
                            long int sent = send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                            if(sent < 0) {
                                closeConnection = true;
                                break;
                            }

                        }
                        else if (robot.isWaitingFor() == CLIENT_FULL_POWER) {
//                            if (message.length() != CLIENT_FULL_POWER_MAX_LENGTH) {
//                                throw ServerError(SERVER_SYNTAX_ERROR);
//                            }
                            if (message != CLIENT_FULL_POWER_TEXT) {
                                throw ServerError(SERVER_LOGIC_ERROR);
                            }
                            robot.setCurrentState(robot.getPrevState());
                            // to do
                            
                            
                        }
                        else if (robot.isWaitingFor() == CLIENT_MESSAGE) {
                            if (message.length() > CLIENT_MESSAGE_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                            cout << "SECRET WORD: " << message << endl;
                            reply = robot.makeMessage(SERVER_LOGOUT);
                            send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                            closeConnection = true;
                            break;

                        }

                    }
                    catch (ServerError & se) {
                        reply = robot.makeMessage(se.errorId());
                        send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                        cout << "spojeni bylo uzavreno kvuli chybe: " << se.errorId() << endl;
                        closeConnection = true;
                        break;
                    }


                }
                
                try {
                    
                    string incMsg = robot.getIncompleteMessage();
                    
                    cout << "incmsg: " << incMsg << endl;
                    cout << "status :" << robot.isWaitingFor() << endl;
                    if (incMsg.length()) {
                        if (robot.isWaitingFor() == CLIENT_USERNAME) {
                            if (incMsg.length() > CLIENT_USERNAME_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                        }
                        else if (robot.isWaitingFor() == CLIENT_MESSAGE) {
                            if (incMsg.length() > CLIENT_MESSAGE_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                        }
                        else if(robot.isWaitingFor() == CLIENT_OK) {
                            if (incMsg.length() > CLIENT_OK_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                        }
                        else if (robot.isWaitingFor() == CLIENT_FULL_POWER) {
                            if (incMsg.length() > CLIENT_FULL_POWER_MAX_LENGTH) {
                                throw ServerError(SERVER_SYNTAX_ERROR);
                            }
                            
                        }
                    }
                }
                catch(ServerError & se) {
                    reply = robot.makeMessage(se.errorId());
                    send(c, reply.c_str(), reply.length(), MSG_NOSIGNAL);

                    cout << "spojeni bylo uzavreno kvuli chybe: " << se.errorId() << endl;
                    closeConnection = true;
                }


                if(closeConnection){
                    cout << "Spojeni se uzavira..." << endl;
                    break;
                }

//                cout << buffer << endl;
            }
            close(c);
            break;
        }

        // Aby nam nezustavaly v systemu zombie procesy po kazdem obslouzeneme klientovi,
        // je nutne otestovat, zda se uz nejaky podproces ukoncil.
        // Prvni argument rika, cekej na jakykoliv proces potomka, treti argument zajisti,
        // ze je funkce neblokujici (standardne blokujici je, coz ted opravdu nechceme).
        int status = 0;

        waitpid(0, &status, WNOHANG);

        close(c); // Nove vytvoreny socket je nutne zavrit v hlavnim procesu, protoze by na nem v systemu
        // zustala reference a jeho zavreni v novem procesu by nemelo zadny efekt.
    }

    close(l);
    return 0;
}
