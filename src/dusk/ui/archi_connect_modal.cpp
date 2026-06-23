#include <dusk/ui/archi_connect_modal.hpp>
#include <dusk/ui/string_button.hpp>

#include <thread>
#include "dusk/archipelago/archipelago_context.hpp"
#include "m_Do/m_Do_audio.h"

namespace dusk::ui {


static std::atomic connectStatus = ArchiConnectModal::ConnectionStatus::None;

void CreateSetupConnectionInfoModal();

ArchiConnectModal::ArchiConnectModal() :
    Modal({
       .title = "Archipelago",
       .bodyRml = "Connecting to Server...",
       .onDismiss = [this](Modal& modal) {
           mDoAud_seStartMenu(kSoundWindowClose);
           modal.pop(false);
       },
       .icon = "verifying",
    }) {
    mRoot->SetProperty("white-space", "pre-line");
}

void ArchiConnectModal::update() {
    Modal::update();

    auto currentStatus = connectStatus.load();

    if (currentStatus != mDisplayedStatus) {
        mDisplayedStatus = currentStatus;

        if (currentStatus == ConnectionStatus::Success ||
        currentStatus == ConnectionStatus::Error)
        {
            if (currentStatus == ConnectionStatus::Success) {
                mDoAud_seStartMenu(kSoundSeedGenerateSuccess);
                set_icon("celebration");
                set_body("Sucessfully Connected to server!");
            } else {
                mDoAud_seStartMenu(kSoundSeedGenerateError);
                set_icon("error");
                set_body("Failed to Connect to server.");
            }
            add_action({
                .label = "OK",
                .onPressed = [currentStatus](Modal& modal) {
                    mDoAud_seStartMenu(kSoundWindowClose);
                    modal.pop(false);

                    // show connection setup modal on failure
                    if (currentStatus == ConnectionStatus::Error) {
                        CreateSetupConnectionInfoModal();
                    }
                }
            });

            // Refocus so that we focus the new button
            focus();

            mDisplayedStatus = ConnectionStatus::Ready;
        }else if (currentStatus == ConnectionStatus::Generating) {
            set_body("Loading seed data into game...");
        }else if (currentStatus == ConnectionStatus::Disconnecting) {
            set_body("Disconnecting from previous server...");
        }

        connectStatus.store(mDisplayedStatus);
    }
}

void HandleArchipelagoConnect() {
    // if a connection was already established, disconnect before attempting a new connection.
    if (archi::ArchipelagoContext::IsConnected()) {
        connectStatus.store(ArchiConnectModal::ConnectionStatus::Disconnecting);
        archi::ArchipelagoContext::DisconnectFromServer();
    }

    if (!archi::ArchipelagoContext::ConnectToServer(true)) {
        archi::ArchipelagoContext::DisconnectFromServer();
        connectStatus.store(ArchiConnectModal::ConnectionStatus::Error);
        return;
    }

    connectStatus.store(ArchiConnectModal::ConnectionStatus::Generating);

    while (!archi::ArchipelagoContext::IsReceivedLocationScouts()) {
        std::this_thread::yield();
    }

    archi::ArchipelagoContext::GenerateLocalWorldData();

    connectStatus.store(ArchiConnectModal::ConnectionStatus::Success);
}

void ConnectAndLoadArchipelago() {
    connectStatus.store(ArchiConnectModal::ConnectionStatus::Connecting);
    std::thread archiConnectThread(HandleArchipelagoConnect);
    archiConnectThread.detach();

    push_document(std::make_unique<ArchiConnectModal>());

    if (auto* doc = top_document()) {
        doc->focus();
    }
}

void CreateSetupConnectionInfoModal() {
    auto& doc = push_document(std::make_unique<MultiTextInputModal>(Modal::Props{
        .title = "Connection Info",
        .bodyRml = "",
        .actions = {
            ModalAction{
                .label = "Connect",
                .onPressed = [](Modal& modal) {
                    auto textModal = dynamic_cast<MultiTextInputModal*>(&modal);

                    archi::ArchipelagoContext::SetServerIp(textModal->get_input_text(0));
                    archi::ArchipelagoContext::SetPassword(textModal->get_input_text(1));
                    archi::ArchipelagoContext::SetSlotName(textModal->get_input_text(2));

                    modal.pop(false);

                    ConnectAndLoadArchipelago();
                },
            },
            ModalAction{
                .label = "Cancel",
                .onPressed = [](Modal& modal) {
                    // TODO: cancelling this modal will still progress file select to starting the save,
                    // which could potentially allow a user to break their save file.

                    modal.pop(false);
                },
            },
        },
        .icon = "information"
    }));
    auto inputModal = dynamic_cast<MultiTextInputModal*>(&doc);

    inputModal->add_input_text("Server IP", archi::ArchipelagoContext::GetServerIp());
    inputModal->add_input_text("Password", archi::ArchipelagoContext::GetPassword());
    inputModal->add_input_text("Slot Name", archi::ArchipelagoContext::GetSlotName());
}

void BeginArchipelagoConnectionUI(bool forceChangeConnection) {
    if (forceChangeConnection) {
        CreateSetupConnectionInfoModal();
        return;
    }

    // TODO: associate server connection info to save slots
    bool hasConnectInfo = (!archi::ArchipelagoContext::GetServerIp().empty() && !archi::ArchipelagoContext::GetSlotName().empty());

    if (hasConnectInfo) {
        ConnectAndLoadArchipelago();
    }else {
        CreateSetupConnectionInfoModal();
    }
}
}