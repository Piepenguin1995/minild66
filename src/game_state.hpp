#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <memory>

#include "entity_manager.hpp"

// I should really just make a library with this definition,
// almost everything I write seems to use it
class GameState : public sf::Drawable
{
protected:
    std::shared_ptr<GameState>& mState;
    std::shared_ptr<GameState>& mPrevState;
    EntityManager* mMgr;

public:
    virtual void handleEvent(const sf::Event& event,
            const sf::RenderWindow& window) = 0;
    virtual void handleInput(float dt,
            const sf::RenderWindow& window) = 0;
    virtual void update(float dt) = 0;

    GameState(std::shared_ptr<GameState>& state,
            std::shared_ptr<GameState>& prevState,
            EntityManager* mgr) :
        mState(state),
        mPrevState(prevState),
        mMgr(mgr)
    {
    }

    virtual ~GameState() {}
};

#endif /* GAME_STATE_HPP */
