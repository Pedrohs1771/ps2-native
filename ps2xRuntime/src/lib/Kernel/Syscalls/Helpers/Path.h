namespace
{
    constexpr char kMc0Prefix[] = "mc0:";

    std::string toLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
        });
        return value;
    }

    std::string stripIsoVersionSuffix(std::string value)
    {
        const std::size_t semicolon = value.find(';');
        if (semicolon == std::string::npos)
        {
            return value;
        }

        bool numericSuffix = semicolon + 1 < value.size();
        for (std::size_t i = semicolon + 1; i < value.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(value[i])))
            {
                numericSuffix = false;
                break;
            }
        }

        if (numericSuffix)
        {
            value.erase(semicolon);
        }
        return value;
    }

    std::string normalizePs2PathSuffix(std::string suffix)
    {
        std::replace(suffix.begin(), suffix.end(), '\\', '/');
        suffix = stripIsoVersionSuffix(std::move(suffix));
        while (!suffix.empty() && (suffix.front() == '/' || suffix.front() == '\\'))
        {
            suffix.erase(suffix.begin());
        }
        return suffix;
    }

    std::string normalizeIsoCdPathSuffix(std::string suffix)
    {
        std::replace(suffix.begin(), suffix.end(), '\\', '/');
        while (!suffix.empty() && (suffix.front() == '/' || suffix.front() == '\\'))
        {
            suffix.erase(suffix.begin());
        }
        std::string normalized;
        std::size_t componentStart = 0u;
        while (componentStart <= suffix.size())
        {
            const std::size_t componentEnd = suffix.find('/', componentStart);
            const std::size_t end = componentEnd == std::string::npos ? suffix.size() : componentEnd;
            normalized += stripIsoVersionSuffix(suffix.substr(componentStart, end - componentStart));
            if (componentEnd == std::string::npos)
            {
                break;
            }
            normalized.push_back('/');
            componentStart = componentEnd + 1u;
        }
        return normalized;
    }

    std::filesystem::path resolveCdPathCaseInsensitive(
        const std::filesystem::path &base,
        const std::string &suffix)
    {
        const std::string normalizedSuffix = normalizeIsoCdPathSuffix(suffix);
        std::filesystem::path current = base.lexically_normal();

        for (const std::filesystem::path &componentPath : std::filesystem::path(normalizedSuffix))
        {
            const std::string component = componentPath.string();
            if (component.empty() || component == ".")
            {
                continue;
            }
            if (component == "..")
            {
                current /= componentPath;
                current = current.lexically_normal();
                continue;
            }

            std::error_code error;
            if (!std::filesystem::is_directory(current, error) || error)
            {
                current /= componentPath;
                continue;
            }

            std::filesystem::path foldedMatch;
            std::size_t foldedMatchCount = 0u;
            bool exactMatch = false;
            std::filesystem::directory_iterator iterator(current, error);
            const std::filesystem::directory_iterator end;
            while (!error && iterator != end)
            {
                const std::string candidate = iterator->path().filename().string();
                if (candidate == component)
                {
                    current /= iterator->path().filename();
                    exactMatch = true;
                    break;
                }
                if (toLowerAscii(candidate) == toLowerAscii(component))
                {
                    foldedMatch = iterator->path().filename();
                    ++foldedMatchCount;
                }
                iterator.increment(error);
            }

            if (!exactMatch && foldedMatchCount == 1u)
            {
                current /= foldedMatch;
            }
            else if (!exactMatch)
            {
                current /= componentPath;
            }
        }

        return current.lexically_normal();
    }

    std::filesystem::path getConfiguredHostRoot()
    {
        const PS2Runtime::IoPaths &paths = PS2Runtime::getIoPaths();
        if (!paths.hostRoot.empty())
        {
            return paths.hostRoot;
        }
        if (!paths.elfDirectory.empty())
        {
            return paths.elfDirectory;
        }

        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path(".") : cwd.lexically_normal();
    }

    std::filesystem::path getConfiguredCdRoot()
    {
        const PS2Runtime::IoPaths &paths = PS2Runtime::getIoPaths();
        if (!paths.cdRoot.empty())
        {
            return paths.cdRoot;
        }
        if (!paths.elfDirectory.empty())
        {
            return paths.elfDirectory;
        }

        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path(".") : cwd.lexically_normal();
    }

    std::filesystem::path getConfiguredMcRoot()
    {
        const PS2Runtime::IoPaths &paths = PS2Runtime::getIoPaths();
        if (!paths.mcRoot.empty())
        {
            return paths.mcRoot;
        }
        if (!paths.elfDirectory.empty())
        {
            return paths.elfDirectory / "mc0";
        }

        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path("mc0") : (cwd / "mc0").lexically_normal();
    }
}
