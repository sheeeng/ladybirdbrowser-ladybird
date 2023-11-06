/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/LexicalPath.h>
#include <AK/OwnPtr.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/LocalServer.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibIPC/SingleServer.h>
#include <LibMain/Main.h>
#include <LibTLS/Certificate.h>
#include <RequestServer/ConnectionFromClient.h>
#include <RequestServer/GeminiProtocol.h>
#include <RequestServer/HttpProtocol.h>
#include <RequestServer/HttpsProtocol.h>

// FIXME: Share b/w RequestServer and WebSocket
ErrorOr<String> find_certificates(StringView serenity_resource_root)
{
    auto cert_path = TRY(String::formatted("{}/res/ladybird/cacert.pem", serenity_resource_root));
    if (!FileSystem::exists(cert_path)) {
        auto app_dir = LexicalPath::dirname(TRY(Core::System::current_executable_path()));

        cert_path = TRY(String::formatted("{}/cacert.pem", LexicalPath(app_dir).parent()));
        if (!FileSystem::exists(cert_path))
            return Error::from_string_view("Don't know how to load certs!"sv);
    }
    return cert_path;
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    int fd_passing_socket { -1 };
    StringView serenity_resource_root;

    Core::ArgsParser args_parser;
    args_parser.add_option(fd_passing_socket, "File descriptor of the fd passing socket", "fd-passing-socket", 'c', "fd-passing-socket");
    args_parser.add_option(serenity_resource_root, "Absolute path to directory for serenity resources", "serenity-resource-root", 'r', "serenity-resource-root");
    args_parser.parse(arguments);

    // Ensure the certificates are read out here.
    DefaultRootCACertificates::set_default_certificate_path(TRY(find_certificates(serenity_resource_root)));
    [[maybe_unused]] auto& certs = DefaultRootCACertificates::the();

    Core::EventLoop event_loop;

    [[maybe_unused]] auto gemini = make<RequestServer::GeminiProtocol>();
    [[maybe_unused]] auto http = make<RequestServer::HttpProtocol>();
    [[maybe_unused]] auto https = make<RequestServer::HttpsProtocol>();

    auto client = TRY(IPC::take_over_accepted_client_from_system_server<RequestServer::ConnectionFromClient>());
    client->set_fd_passing_socket(TRY(Core::LocalSocket::adopt_fd(fd_passing_socket)));

    auto result = event_loop.exec();

    // FIXME: We exit instead of returning, so that protocol destructors don't get called.
    //        The Protocol base class should probably do proper de-registration instead of
    //        just VERIFY_NOT_REACHED().
    exit(result);
}
