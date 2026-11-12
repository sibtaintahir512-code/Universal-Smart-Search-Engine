#include "TestHarness.hpp"
#include "gui/ThemeManager.hpp"

namespace sse::test {

void runThemeManagerTests() {
    std::cout << "\n=== Running ThemeManager Unit Tests ===" << std::endl;

    // Test 1: Initial theme
    {
        gui::ThemeManager::applyTheme(gui::ThemeMode::Dark);
        TEST_ASSERT_EQ(static_cast<int>(gui::ThemeManager::currentTheme()), static_cast<int>(gui::ThemeMode::Dark));
    }

    // Test 2: Dark Stylesheet token verification
    {
        QString darkQss = gui::ThemeManager::getDarkStyleSheet();
        TEST_ASSERT(!darkQss.isEmpty());
        TEST_ASSERT(darkQss.contains("#1E1E24")); // Charcoal background
        TEST_ASSERT(darkQss.contains("#2A2B36")); // Slate surface
        TEST_ASSERT(darkQss.contains("#4F46E5")); // Indigo accent
        TEST_ASSERT(darkQss.contains("#F3F4F6")); // Text color
    }

    // Test 3: Light Stylesheet token verification
    {
        QString lightQss = gui::ThemeManager::getLightStyleSheet();
        TEST_ASSERT(!lightQss.isEmpty());
        TEST_ASSERT(lightQss.contains("#F8FAFC")); // Light background
        TEST_ASSERT(lightQss.contains("#FFFFFF")); // Surface
        TEST_ASSERT(lightQss.contains("#4F46E5")); // Indigo accent
        TEST_ASSERT(lightQss.contains("#0F172A")); // Slate dark text
    }

    // Test 4: Theme toggling
    {
        gui::ThemeManager::applyTheme(gui::ThemeMode::Dark);
        TEST_ASSERT_EQ(static_cast<int>(gui::ThemeManager::currentTheme()), static_cast<int>(gui::ThemeMode::Dark));

        gui::ThemeManager::toggleTheme();
        TEST_ASSERT_EQ(static_cast<int>(gui::ThemeManager::currentTheme()), static_cast<int>(gui::ThemeMode::Light));

        gui::ThemeManager::toggleTheme();
        TEST_ASSERT_EQ(static_cast<int>(gui::ThemeManager::currentTheme()), static_cast<int>(gui::ThemeMode::Dark));
    }
}

} // namespace sse::test
