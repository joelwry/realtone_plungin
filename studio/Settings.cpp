#include "Settings.h"

#include "Paths.h"

namespace realtone::studio
{

Settings& Settings::get()
{
    static Settings instance;
    return instance;
}

Settings::Settings() : file_(userDataDirectory().getChildFile("settings.xml")) { load(); }

void Settings::useFile(const juce::File& file)
{
    const juce::ScopedLock sl(lock_);
    file_ = file;
    load();
}

void Settings::load()
{
    tree_ = juce::ValueTree("Settings");
    if (!file_.existsAsFile())
        return;
    if (auto xml = juce::parseXML(file_))
    {
        auto loaded = juce::ValueTree::fromXml(*xml);
        if (loaded.isValid() && loaded.hasType("Settings"))
            tree_ = loaded;
    }
}

void Settings::save()
{
    file_.getParentDirectory().createDirectory();
    if (auto xml = tree_.createXml())
    {
        // Write to a sibling and move over, so a crash never leaves a truncated settings file.
        const auto tmp = file_.withFileExtension("tmp");
        if (xml->writeTo(tmp))
            tmp.moveFileTo(file_);
    }
}

juce::String Settings::getString(const juce::String& key, const juce::String& fallback)
{
    const juce::ScopedLock sl(lock_);
    return tree_.hasProperty(key) ? tree_.getProperty(key).toString() : fallback;
}

int Settings::getInt(const juce::String& key, int fallback)
{
    const juce::ScopedLock sl(lock_);
    return tree_.hasProperty(key) ? static_cast<int>(tree_.getProperty(key)) : fallback;
}

double Settings::getDouble(const juce::String& key, double fallback)
{
    const juce::ScopedLock sl(lock_);
    return tree_.hasProperty(key) ? static_cast<double>(tree_.getProperty(key)) : fallback;
}

bool Settings::getBool(const juce::String& key, bool fallback)
{
    const juce::ScopedLock sl(lock_);
    return tree_.hasProperty(key) ? static_cast<bool>(tree_.getProperty(key)) : fallback;
}

void Settings::setString(const juce::String& key, const juce::String& value)
{
    const juce::ScopedLock sl(lock_);
    tree_.setProperty(key, value, nullptr);
    save();
}

void Settings::setInt(const juce::String& key, int value)
{
    const juce::ScopedLock sl(lock_);
    tree_.setProperty(key, value, nullptr);
    save();
}

void Settings::setDouble(const juce::String& key, double value)
{
    const juce::ScopedLock sl(lock_);
    tree_.setProperty(key, value, nullptr);
    save();
}

void Settings::setBool(const juce::String& key, bool value)
{
    const juce::ScopedLock sl(lock_);
    tree_.setProperty(key, value, nullptr);
    save();
}

juce::StringArray Settings::getList(const juce::String& key)
{
    const juce::ScopedLock sl(lock_);
    juce::StringArray out;
    if (auto child = tree_.getChildWithName(key); child.isValid())
        for (int i = 0; i < child.getNumChildren(); ++i)
            out.add(child.getChild(i).getProperty("v").toString());
    return out;
}

void Settings::setList(const juce::String& key, const juce::StringArray& values)
{
    const juce::ScopedLock sl(lock_);
    tree_.removeChild(tree_.getChildWithName(key), nullptr);
    juce::ValueTree list(key);
    for (const auto& v : values)
        list.appendChild(juce::ValueTree("i", {{"v", v}}), nullptr);
    tree_.appendChild(list, nullptr);
    save();
}

juce::File Settings::libraryRoot()
{
    const auto s = getString(keys::libraryRoot);
    return s.isNotEmpty() ? juce::File(s) : defaultLibraryDirectory();
}

void Settings::setLibraryRoot(const juce::File& dir) { setString(keys::libraryRoot, dir.getFullPathName()); }

int Settings::sampleMemoryLimitMB() { return juce::jlimit(256, 16384, getInt(keys::sampleMemoryMB, 3072)); }

}  // namespace realtone::studio
