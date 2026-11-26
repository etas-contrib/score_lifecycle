/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

// Started by the launch manager as an unprivileged user with the working directory /tmp/locked/visible, whose
// parent /tmp/locked is only accessible to root (prepared by control_client_test_driver). Neither the absolute nor
// the cwd-relative path of secret.txt may be opened. The result is reported via the exit code, as this process cannot
// write to its working directory: the launch manager only reports the Run Target as activated if it exits with 0.

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{

constexpr int kAbsoluteOpenNotDenied = 2;
constexpr int kRelativeOpenNotDenied = 3;

/// @brief Tries to open @p path and prints the result.
/// @return true if the open was denied with EACCES
bool openIsDenied(const char* path)
{
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
    {
        const int error_number = errno;
        std::cout << "open(\"" << path << "\") failed: " << std::strerror(error_number) << std::endl;
        return error_number == EACCES;
    }

    std::string content;
    char buffer[256];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(fd, buffer, sizeof(buffer))) > 0)
    {
        content.append(buffer, static_cast<std::size_t>(bytes_read));
    }
    static_cast<void>(::close(fd));

    std::cout << "open(\"" << path << "\") succeeded, content: " << content << std::endl;
    return false;
}

}  // namespace

int main()
{
    std::cout << "cwd_access_process running as uid " << ::getuid() << ", gid " << ::getgid() << std::endl;

    // Sanity check that the test setup locks the file for this user
    if (!openIsDenied("/tmp/locked/visible/secret.txt"))
    {
        std::cout << "FAIL: the absolute path is accessible, the test setup is broken" << std::endl;
        return kAbsoluteOpenNotDenied;
    }

    // The working directory must not grant access to the locked directory tree
    if (!openIsDenied("secret.txt"))
    {
        std::cout << "FAIL: the working directory gives access to a file the process must not access" << std::endl;
        return kRelativeOpenNotDenied;
    }

    std::cout << "PASS: secret.txt is not accessible" << std::endl;
    return EXIT_SUCCESS;
}
