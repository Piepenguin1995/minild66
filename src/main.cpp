#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <JsonBox.h>

#include "constants.hpp"
#include "game_state.hpp"
#include "game_state_title.hpp"
#include "game_state_game.hpp"
#include "entity_manager.hpp"
#include "network_manager.hpp"
#include "game_container.hpp"

class Tileset;
class GameMap;
class Character;

// Easiest way to ensure a otherwise infinite looping function
// will halt is to use a pointer as a kill-switch
void pollNetworkEvents(NetworkManager* nmgr, bool* kill)
{
    // Run until killed, accepting connections and parsing them
    // as network events. The events are available from nmgr
    // using pollEvent, which should be called in the main thread
    while(!*kill)
    {
        nmgr->waitEvent();
    }
}

int main(int argc, char* argv[])
{
    // Check for a server or a client
    // Should really make two different executables, but
    // there's not really enough time for that
    // TODO: Implement arguments instead of this
    if(argc > 1)
    {
        ld::isServer = true;
    }

    std::srand(std::time(nullptr));

    EntityManager entityManager;
    // Load entities
    entityManager.load<Tileset>("tilesets.json");
    entityManager.load<GameMap>("game_map.json");
    entityManager.load<Character>("characters.json");

    // Load network manager
    JsonBox::Value configFile;
    configFile.loadFromFile("config.json");
    NetworkManager networkManager(configFile[ld::isServer ? "server" : "client"]);

    // Open a thread for listening to incoming connections
    bool killPollNetworkThread = false;
    std::thread pollNetworkThread(pollNetworkEvents, &networkManager, &killPollNetworkThread);

    //////////////////////////////////////////////////////////////////
    // SERVER
    //////////////////////////////////////////////////////////////////
    if(ld::isServer)
    {
        bool running = true;
        // Currently connected clients
        // TODO: Identifying by IP is not a safe way to do this, time of
        // connection may be better; they are guaranteed to be unique
        // among all clients since the server receives events
        // serially and not concurrently.
        auto ipCmp = [](const sf::IpAddress& a, const sf::IpAddress& b)
        {
            return a.toInteger() < b.toInteger();
        };
        struct ClientInfo {
            sf::IpAddress ip;
            sf::Uint16 port;
            sf::Uint16 gameId;
            sf::Uint8 charId;
        };
        std::map<sf::IpAddress, ClientInfo, decltype(ipCmp)> connectedClients(ipCmp);
        // Games currently running on this server
        std::map<sf::Uint16, GameContainer> games;

        while(running)
        {
            NetworkManager::Event netEvent;
            while(networkManager.pollEvent(netEvent))
            {
                switch(netEvent.type)
                {
                    default:
                    case NetworkManager::Event::Nop:
                    {
                        break;
                    }
                    ///////////////////////////////////////////////////
                    // CONNECT
                    ///////////////////////////////////////////////////
                    case NetworkManager::Event::Connect:
                    {
                        auto& e = netEvent.connect;
                        if(connectedClients.count(e.sender) > 0)
                        {
                            servout << e.sender.toString() << " is already connected" << std::endl;
                        }
                        else
                        {
                            // If the game doesn't exist yet, make it
                            if(games.count(e.gameId) == 0)
                            {
                                games[e.port] = GameContainer();
                            }
                            GameContainer& game = games[e.port];
                            // Attempt to add to a team
                            // TODO: Add team choosing
                            sf::Uint8 charId = 255;
                            if(game.add("character_fighter", GameContainer::Team::Any, &entityManager, charId))
                            {
                                // Added to the team, send a success
                                // message to connected clients
                                servout << e.sender.toString() << " has connected to game "
                                    << e.gameId << std::endl;
                                // Add to the list of connected clients
                                connectedClients[e.sender] = (ClientInfo){
                                    .ip = e.sender,
                                    .port = e.port,
                                    .gameId = e.gameId,
                                    .charId = charId
                                };
                                // TODO: Send to clients on a need-to-know basis
                                for(auto c : connectedClients)
                                {
                                    e.team = game.characters[charId].team;
                                    networkManager.send(netEvent, c.second.ip, c.second.port);
                                }
                            }
                            else
                            {
                                // Couldn't add, so respond with
                                // a failure message
                                // TODO: When you're 8 indents in,
                                // it's probably time to refactor
                                NetworkManager::Event response;
                                response.gameFull = {
                                    .sender = e.sender,
                                    .port = e.port,
                                    .gameId = e.gameId
                                };
                                response.type = NetworkManager::Event::GameFull;
                                servout << "Game " << e.gameId
                                    << " is full, rejecting with message" << std::endl;
                                networkManager.send(response, e.sender, e.port);
                            }
                        }
                        break;
                    }
                    ///////////////////////////////////////////////////
                    // DISCONNECT
                    ///////////////////////////////////////////////////
                    case NetworkManager::Event::Disconnect:
                    {
                        // Terribly unsecure way of killing server gracefully
                        if(netEvent.disconnect.gameId == 65535 &&
                            netEvent.disconnect.charId == 255)
                        {
                            running = false;
                        }
                        // Same as a connect event but in reverse
                        auto e = netEvent.disconnect;
                        if(connectedClients.count(e.sender) == 0)
                        {
                            servout << e.sender.toString() << " is not connected" << std::endl;
                        }
                        // TODO: This section should also be triggered if a client has not been
                        // heard from for a certain amount of time
                        else
                        {
                            servout << e.sender.toString() << " has disconnected" << std::endl;
                            // Delete from the set of connected clients
                            connectedClients.erase(e.sender);
                            // Broacast
                            for(auto c : connectedClients)
                            {
                                networkManager.send(netEvent, c.second.ip, c.second.port);
                            }
                        }

                        break;
                    }
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////
    // CLIENT
    //////////////////////////////////////////////////////////////////
    else
    {
        // Create the window
        sf::RenderWindow window(sf::VideoMode(ld::width, ld::height), ld::title,
                sf::Style::Titlebar | sf::Style::Close);

        // Disable key repeating
        window.setKeyRepeatEnabled(false);
        
        // Clamp framerate
        window.setVerticalSyncEnabled(true);

        // Window viewport
        sf::View view(sf::FloatRect(0, 0, ld::width, ld::height));
        view.setCenter(ld::width / 2.0f, ld::height / 2.0f);
        window.setView(view);

        // Pointer to current game state
        std::shared_ptr<GameState> state;

        // Load initial game state
        state.reset(new GameStateTitle(state, state, &entityManager));
        // state.reset(new GameStateGame(state, state, &entityManager));

        // Frame time
        sf::Clock clock;

        // Attempt to connect to server
        // TODO: Make this more robust
        auto cts_client = configFile["client"].getObject();
        auto cts_target = cts_client["target"].getObject();
        if(!networkManager.connectToServer(
            cts_target["address"].getString(),
            cts_target["port"].getInteger(),
            cts_target["game"].getInteger()))
        {
            window.close();
        }
        bool hasConnectedToServer = false;

        // Game loop
        while(window.isOpen())
        {
            float dt = clock.restart().asSeconds();

            // Handle events
            sf::Event event;
            while(window.pollEvent(event))
            {
                if(event.type == sf::Event::Closed)
                {
                    window.close();
                }
                if(state != nullptr) state->handleEvent(event, window);
            }
            // Handle network events
            NetworkManager::Event netEvent;
            while(networkManager.pollEvent(netEvent))
            {
                switch(netEvent.type)
                {
                    default:
                    case NetworkManager::Event::Nop:
                    {
                        break;
                    }
                    case NetworkManager::Event::Connect:
                    {
                        auto e = netEvent.connect;
                        clntout << e.sender.toString() << " has connected" << std::endl;
                        if(!hasConnectedToServer && e.sender == networkManager.getIp())
                        {
                            hasConnectedToServer = true;
                            clntout << "\tThat's me, changing game state" << std::endl;
                            clntout << "\tAdded to team " << static_cast<int>(e.team) << std::endl;
                            // Make a new GameContainer
                            std::shared_ptr<GameContainer> game(new GameContainer(
                                    entityManager.getEntity<GameMap>("gamemap_large"),
                                    e.charId));
                            // Add the client to the game, with the
                            // position and team as given by the server
                            game->add("character_fighter", e.team, &entityManager, e.charId);
                            // TODO: Add other players
                            state.reset(new GameStateGame(state, state, game, &entityManager));
                        }
                        break;
                    }
                    case NetworkManager::Event::Disconnect:
                    {
                        auto e = netEvent.disconnect;
                        clntout << e.sender.toString() << " has disconnected" << std::endl;
                        break;
                    }
                    case NetworkManager::Event::GameFull:
                    {
                        auto e = netEvent.gameFull;
                        clntout << e.gameId << " is full, sorry" << std::endl;
                        window.close();
                        break;
                    }
                }
            }

            if(state != nullptr)
            {
                // Update window
                state->handleInput(dt, window);
                state->update(dt);
                // Draw window
                window.clear(sf::Color::Black);
                window.draw(*state);
                window.display();
            }
            else
            {
                window.close();
            }
        }
    }

    // Attempt to disconnect from server
    networkManager.disconnectFromServer();

    // Kill the network thread and join
    killPollNetworkThread = true;
    pollNetworkThread.join();

    return 0;
}
