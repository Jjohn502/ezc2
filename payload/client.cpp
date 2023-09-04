#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <dirent.h>
#include <nlohmann/json.hpp>
using namespace std;


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
    task_json["command"] = "process list";

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
    cout << task_json.dump(4) << endl;
    return task_json;
}

int main(){
    
    const char* server_host ="127.0.0.1";
    const int server_port = 5000;

    //create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        cerr << "Error creating socket" << endl;
        return 1;
    }

    //server addr
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, server_host, &server_address.sin_addr);
    server_address.sin_port = htons(server_port);

    //connect
    if(connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Error connecting to server" << endl;
        return 1;
    }

    const char* request = "GET /api/agent/tasks/pending/2 HTTP/1.1\r\nHOST: localhost\r\nConnection: close\r\n\r\n";
    if(send(sock, request, strlen(request), 0) < 0){
        cerr << "Error Sending request" << endl;
        return 1;
    }

    //Receive response
    char buffer[4096];
    ssize_t bytes_received;
    string response_data;
    while((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0){
        buffer[bytes_received] = '\0'; // null-terminate
        response_data += buffer;
    }
    cout << "Received Data: \n" << response_data << endl;
    close(sock);


    //parse the JSON
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

                cout << "Task ID: " << task_id << ", Agent ID: " << agent_id 
                     << ", Task Name: " << command << ", Timestamp: " << timestamp << endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Could not find the start of the JSON content." << std::endl;
    }
    
    nlohmann::json psJson = ps_list(123, 5);

    
    
    return 0;

}