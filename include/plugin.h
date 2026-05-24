#pragma once

#include <endstone/endstone.hpp>
#include <memory>

class EventListener;
class InventoryUIService;

class InventoryUIPlugin : public endstone::Plugin {
public:
    void onEnable() override;
    void onDisable() override;

private:
    std::unique_ptr<EventListener> listener_;
    std::shared_ptr<InventoryUIService> service_;
};
