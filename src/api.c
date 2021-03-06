#include "../headers/header.h"

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) return 0;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

bool isHttpError(long http_code){
    return !(http_code >= 200 && http_code < 400);
}

char* getRequest(const char* path, long* response_code){
    CURL *curl_handle;
    CURLcode code;
    bool error = false;

    // Init the request memory
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    // Init request settings
    char* url = malloc((31 + 21 + (MAX_NAME_SIZE * 2)) * sizeof(char));
    strcpy(url, "http://localhost/server/api.php");
    strcat(url, path);
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ZephyrQuest-C/1.0");

    // Send the request
    code = curl_easy_perform(curl_handle);

    // Get response code
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, response_code);

    // Check errors
    if(code != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(code));
        error = true;
    }

    // Make a cleanup
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    // Return the request result
    if(!error) {
        char* res = malloc(chunk.size * sizeof(char));
        strcpy(res, chunk.memory);
        free(chunk.memory);
        return res;
    }

    free(chunk.memory);
    return NULL;
}

bool postRequest(const char* path, const char* post_data){
    CURL *curl;
    CURLcode res;
    long response_code;

    // Init the request memory
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char* url = malloc((31 + 21 + (MAX_NAME_SIZE * 2)) * sizeof(char));
    strcpy(url, "http://localhost/server/api.php");
    strcat(url, path);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ZephyrQuest-C/1.0");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if(res != CURLE_OK){
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return false;
        }
        curl_easy_cleanup(curl);
    }

    return !isHttpError(response_code);
}

Map* * getAllMaps(size_t* nb_maps){
    // Get maps in json
    long http_code;
	char* json_str = getRequest("/maps", &http_code);
    if(json_str == NULL || isHttpError(http_code)) {
        *nb_maps = 0;
        return NULL;
    }

    // Init props
	struct json_object *parsed_json, *map, *name, *author, *solvable, *items, 
    *item, *id, *x, *y, *usages, *usage, *usage_id, *usage_x, *usage_y;

    // Parse the json
	parsed_json = json_tokener_parse(json_str);
	
    // Loop through the maps array
    *nb_maps = json_object_array_length(parsed_json);
    Map* * maps = (Map* *) malloc((*nb_maps) * sizeof(Map*));
    if (maps == NULL) exit(EXIT_FAILURE);
    for (size_t i = 0; i < *nb_maps; i++){
        map = json_object_array_get_idx(parsed_json, i);
        json_object_object_get_ex(map, "name", &name);
        json_object_object_get_ex(map, "author", &author);
        json_object_object_get_ex(map, "solvable", &solvable);
        json_object_object_get_ex(map, "items", &items);

        char* final_name = malloc(MAX_NAME_SIZE * sizeof(char));
        strcpy(final_name, json_object_get_string(name));
        char* final_author = malloc(MAX_NAME_SIZE * sizeof(char));
        strcpy(final_author, json_object_get_string(author));
        Map* new_map = createMap(final_name, final_author);
        new_map->solvable = json_object_get_boolean(solvable);
        
        for (size_t j = 0; j < json_object_array_length(items); j++){
            item = json_object_array_get_idx(items, j);
            json_object_object_get_ex(item, "id", &id);
            json_object_object_get_ex(item, "x", &x);
            json_object_object_get_ex(item, "y", &y);
            json_object_object_get_ex(item, "usages", &usages);

            Frame* new_frame = createFrame(
                json_object_get_int(x),
                json_object_get_int(y),
                json_object_get_int(id)
            );
            
            for (size_t k = 0; k < json_object_array_length(usages); k++){
                usage = json_object_array_get_idx(usages, k);
                json_object_object_get_ex(usage, "id", &usage_id);
                json_object_object_get_ex(usage, "x", &usage_x);
                json_object_object_get_ex(usage, "y", &usage_y);

                Frame* new_usage = createFrame(
                    json_object_get_int(usage_x),
                    json_object_get_int(usage_y),
                    json_object_get_int(usage_id)
                );
                appendAtList(new_frame->usages, new_usage);
            }

            addFrameInMap(new_map, new_frame);
        }

        maps[i] = new_map;
    }
    
    return maps;
}

Map* getMapByName(const char* map_name){
    // Get map in json
    char* url = malloc((19 + MAX_NAME_SIZE) * sizeof(char));
    strcat(url, "/mapByName?name=");
    strcat(url, map_name);
    long http_code;
	char* json_str = getRequest(url, &http_code);
    if(json_str == NULL || isHttpError(http_code)) return NULL;

    // Init props
	struct json_object *parsed_json, *name, *author, *solvable, *items,
    *item, *id, *x, *y, *usages, *usage, *usage_id, *usage_x, *usage_y;

    // Parse the json
	parsed_json = json_tokener_parse(json_str);
    json_object_object_get_ex(parsed_json, "name", &name);
    json_object_object_get_ex(parsed_json, "author", &author);
    json_object_object_get_ex(parsed_json, "solvable", &solvable);
    json_object_object_get_ex(parsed_json, "items", &items);

    char* final_name = malloc(MAX_NAME_SIZE * sizeof(char));
    strcpy(final_name, json_object_get_string(name));
    char* final_author = malloc(MAX_NAME_SIZE * sizeof(char));
    strcpy(final_author, json_object_get_string(author));
    Map* new_map = createMap(final_name, final_author);
    new_map->solvable = json_object_get_boolean(solvable);

    for (size_t j = 0; j < json_object_array_length(items); j++){
        item = json_object_array_get_idx(items, j);
        json_object_object_get_ex(item, "id", &id);
        json_object_object_get_ex(item, "x", &x);
        json_object_object_get_ex(item, "y", &y);
        json_object_object_get_ex(item, "usages", &usages);

        Frame* new_frame = createFrame(
            json_object_get_int(x),
            json_object_get_int(y),
            json_object_get_int(id)
        );
        
        for (size_t k = 0; k < json_object_array_length(usages); k++){
            usage = json_object_array_get_idx(usages, k);
            json_object_object_get_ex(usage, "id", &usage_id);
            json_object_object_get_ex(usage, "x", &usage_x);
            json_object_object_get_ex(usage, "y", &usage_y);

            Frame* new_usage = createFrame(
                json_object_get_int(usage_x),
                json_object_get_int(usage_y),
                json_object_get_int(usage_id)
            );
            appendAtList(new_frame->usages, new_usage);
        }

        addFrameInMap(new_map, new_frame);
    }
    
    return new_map;
    // return NULL;
}

bool uploadNewMap(Map* map){
    if(map == NULL) return false;
	struct json_object *items, *current_item, *usages, *current_json_usage;

    items = json_object_new_array();
    ListElement* i = map->items->first;
    while (i != NULL) {
        Frame* current_frame = i->data;
	    current_item = json_object_new_object();
        json_object_object_add(current_item, "id", json_object_new_int(current_frame->id));
        json_object_object_add(current_item, "x", json_object_new_int(current_frame->x));
        json_object_object_add(current_item, "y", json_object_new_int(current_frame->y));
        
        usages = json_object_new_array();
        ListElement* j = current_frame->usages ->first;
        while (j != NULL) {
            Frame* current_usage = j->data;
	        current_json_usage = json_object_new_object();
            json_object_object_add(current_json_usage, "id", json_object_new_int(current_usage->id));
            json_object_object_add(current_json_usage, "x", json_object_new_int(current_usage->x));
            json_object_object_add(current_json_usage, "y", json_object_new_int(current_usage->y));
            json_object_object_add(current_json_usage, "usages", json_object_new_array());
            json_object_array_add(usages, current_json_usage);
            j = j->next;
        }
        json_object_object_add(current_item, "usages", usages);

        json_object_array_add(items, current_item);
        i = i->next;
    }
    
    const char* map_str = json_object_to_json_string_ext(items, JSON_C_TO_STRING_PLAIN);
    char* path = malloc(21 + (MAX_NAME_SIZE * 2) * sizeof(char));
    strcpy(path, "/newMap?name=");
    strcat(path, map->name);
    strcat(path, "&author=");
    strcat(path, map->author);
    bool upload = postRequest(path, map_str);

    bool validate = false;
    Stack final_sol, actions = initStack();
    bool can_be_solved = checkMap(map, true) && solve(map, &actions, false);
    if(can_be_solved){
        if(countOfStack(&actions) > 1){
            Stack easy_actions = initStack();
            bool is_easier_sol = searchEasySolution(map, &easy_actions, countOfStack(&actions), false);
            if(is_easier_sol && countOfStack(&easy_actions) < countOfStack(&actions))
                final_sol = easy_actions;
            else final_sol = actions;
        } else final_sol = actions;
        Stack path = getBestPath(map, &final_sol, false);
        bool success = setCanBeDone(map, &path);
        validate = success;
    }
    return upload && validate;
}

bool setCanBeDone(Map* map, Stack* actions){
    if(map == NULL) return false;
	struct json_object *solutions, *sol;

    solutions = json_object_new_array();
    while (actions->first != NULL) {
        Coord current_sol = pull(actions);
	    sol = json_object_new_object();
        json_object_object_add(sol, "x", json_object_new_int(current_sol.x));
        json_object_object_add(sol, "y", json_object_new_int(current_sol.y));
        json_object_array_add(solutions, sol);
    }
    
    const char* json = json_object_to_json_string_ext(solutions, JSON_C_TO_STRING_PLAIN);
    char* path = malloc(15 + MAX_NAME_SIZE * sizeof(char));
    strcpy(path, "/validate?name=");
    strcat(path, map->name);
    return postRequest(path, json);
}
