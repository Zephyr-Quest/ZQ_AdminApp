#include "../headers/header.h"

bool moveTo(Map* base_map, Coord* player, Coord destination, bool verbose){
    // Search and print distances
    char distance_map[SIZE_MAP][SIZE_MAP];
    generateMapArray(distance_map, base_map);
    int distance = 1;
    Stack near_points = initStack();
    put(&near_points, *player);

    while(near_points.first != NULL){
        Stack new_near_points = initStack();

        // Loop through the last near points file
        while(near_points.first != NULL){
            Coord current_point = pull(&near_points);

            //Find near points of the current point and print distances on them
            Coord* current_nears = getNearPoints(current_point);
            int i = 0;
            while(current_nears[i].x != SIZE_MAP && current_nears[i].y != SIZE_MAP && i < 4){
                Coord current_near_point = current_nears[i];
                char current_near_value = distance_map[current_near_point.y][current_near_point.x];
                if(canBeHover(current_near_value) && !(isCoordsEquals(current_near_point, *player) || isCoordsEquals(current_near_point, destination))){
                    distance_map[current_near_point.y][current_near_point.x] = distance;
                    put(&new_near_points, current_near_point);
                }
                i++;
            }
        }
        near_points.first = new_near_points.first;
        distance++;
    }
    if(verbose) printMapArray(distance_map, true);

    // Check if a path exists
    bool res = false;
    Coord* nears_endpoint = getNearPoints(destination);
    int i = 0;
    while(isInMap(nears_endpoint[i]) && i < 4){
        Coord current_near = nears_endpoint[i];
        char current_near_value = distance_map[current_near.y][current_near.x];
        if(!canBeHover(current_near_value) && !isObstacle(current_near_value)){
            // A path exists
            res = true;
            player->x = current_near.x;
            player->y = current_near.y;
        }
        i++;
    }

    return res;
}

bool pathfinding(Map* base_map, Coord start, Coord end, bool verbose){
    return moveTo(base_map, &start, end, verbose);
}

Coord* getNearPoints(Coord center){
    Coord* res = (Coord *) malloc(4 * sizeof(Coord));
    int i = 0;
    for (int x = -1; x <= 1; x++){
        for (int y = -1; y <= 1; y++){
            if(abs(x) != abs(y)){
                Coord corner;
                corner.x = center.x + x;
                corner.y = center.y + y;
                if(isInMap(corner)){
                    res[i] = corner;
                    i++;
                }
            }
        }
    }
    if(i < 4){
        Coord end; end.x = SIZE_MAP; end.y = SIZE_MAP;
        res[i] = end;
    }
    return res;
}

List* searchExits(Map* map, Coord player){
    // Get all of closed doors
    List* blocking_doors = initList();
    List* closed_doors = initList();
    ListElement* current = map->items->first;
    while (current != NULL) {
        if (current->data->id == 2 && !current->data->state)
            appendAtList(closed_doors, current->data); 
        current = current->next;
    }

    // Choose which doors are blocking doors
    current = closed_doors->first;
    while (current != NULL) {
        Coord door_coord; door_coord.x = current->data->x; door_coord.y = current->data->y;
        Coord* nears_doorpoints = getNearPoints(door_coord);

        size_t j = 0;
        bool is_blocking = false;
        while(isInMap(nears_doorpoints[j]) && j < 4){
            Coord current_near = nears_doorpoints[j];
            if(locateFrameByCoord(map, current_near, false) == NULL && pathfinding(map, player, current_near, false)) {
                is_blocking = !is_blocking;
            }
            j++;
        }

        if(is_blocking) appendAtList(blocking_doors, current->data);
        current = current->next;
    }

    return blocking_doors;
}

Frame* getDoorLever(Map* map, Frame* door, Coord player, Stack* actions){
    Frame* lever = NULL;
    Coord lever_coord;
    ListElement* current = door->usages->first;
    while (current != NULL && lever == NULL) {
        lever = current->data;
        lever_coord.x = lever->x; lever_coord.y = lever->y;
        if(!pathfinding(map, player, lever_coord, false) || stackContainsFrame(actions, lever)) lever = NULL;
        current = current->next;
    }
    return lever;
}

bool useLever(Map* map, Frame* lever, Coord* player, bool verbose){
    Coord lever_coord;
    lever_coord.x = lever->x;
    lever_coord.y = lever->y;

    // Move the player to the lever
    if(!moveTo(map, player, lever_coord, false)) return false;

    // Open or close doors
    printFrame(lever);
    ListElement* current = lever->usages->first;
    while (current != NULL) {
        Frame* current_door = current->data;
        current_door = locateFrame(map, current_door->x, current_door->y, false);
        printFrame(current_door);
        if(current_door != NULL)
            current_door->state = !current_door->state;
        current = current->next;
    }

    if(verbose) display(map, false);
    return true;
}

bool solve(Map* map, Stack* interactions, bool verbose){
    // Start and end point
    Coord end_point, player;
    player.x = START_X; player.y = START_Y;
    end_point.x = END_X; end_point.y = END_Y;

    bool res = true;    // true means resolvable, false means unresolvable
    size_t nb_actions = 0;
    
    while(!pathfinding(map, player, end_point, false) && res){
        List* blocking_doors = searchExits(map, player);
        bool can_exit = false;
        ListElement* current = blocking_doors->first;
        while (current != NULL && !can_exit) {
            Frame* lever = getDoorLever(map, current->data, player, interactions);
            if(lever != NULL) lever = locateFrame(map, lever->x, lever->y, false);
            if(lever != NULL){
                can_exit = useLever(map, lever, &player, verbose);
                if(can_exit) {
                    if(nb_actions >= MAX_ACTIONS) res = false;
                    else {
                        putFrame(interactions, lever);
                        nb_actions++;
                    }
                }
            }
            current = current->next;
        }
        if(!can_exit) res = false;
    }

    return res;
}