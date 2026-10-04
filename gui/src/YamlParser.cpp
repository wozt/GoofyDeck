#include "YamlParser.h"
#include "Config.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

YamlParser::YamlParser()
{
}

bool YamlParser::parse(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    return parseString(content);
}

bool YamlParser::parseString(const QString &content)
{
    m_pages.clear();
    m_presets.clear();
    
    if (content.isEmpty()) {
        qWarning() << "[YamlParser] Empty content provided";
        return false;
    }
    
    // Simple YAML parsing - extract key values
    QRegularExpression brightnessRe("^brightness:\\s*(\\d+)");
    QRegularExpressionMatch brightnessMatch = brightnessRe.match(content);
    if (brightnessMatch.hasMatch()) {
        m_brightness = brightnessMatch.captured(1).toInt();
    }
    
    // Parse presets section
    parsePresets(content);
    
    // Extract pages - look for "pages:" section
    parsePages(content);
    
    if (m_pages.isEmpty()) {
        qWarning() << "[YamlParser] No pages found in configuration";
        return false;
    }
    
    return true;
}

void YamlParser::parsePresets(const QString &content)
{
    // Find presets: section
    int presetsPos = content.indexOf("presets:");
    if (presetsPos == -1) {
        return;
    }
    
    // Find end of presets section (next top-level section)
    int presetsEnd = content.indexOf(QRegularExpression("\n[a-zA-Z_][a-zA-Z0-9_]*:"), presetsPos + 8);
    if (presetsEnd == -1) {
        presetsEnd = content.length();
    }
    
    QString presetsSection = content.mid(presetsPos, presetsEnd - presetsPos);
    
    // Parse individual presets (2 space indent followed by preset name)
    QRegularExpression presetRe("\\n  ([a-zA-Z_$][a-zA-Z0-9_]*):");
    QRegularExpressionMatchIterator matchIter = presetRe.globalMatch(presetsSection);
    
    while (matchIter.hasNext()) {
        QRegularExpressionMatch match = matchIter.next();
        QString presetName = match.captured(1);
        
        // Store preset name (we can store properties later if needed)
        m_presets[presetName] = QVariantMap();
    }
}

void YamlParser::parsePages(const QString &content)
{
    // Find pages: section
    int pagesPos = content.indexOf("pages:");
    if (pagesPos == -1) {
        return;
    }
    
    QString pagesSection = content.mid(pagesPos);
    
    // Split into lines
    QStringList lines = pagesSection.split('\n');
    
    YamlPage currentPage;
    QString currentButtonSection;
    bool inPage = false;
    bool inButton = false;
    int buttonIndentLevel = 0;
    
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i];
        
        // Check indent level
        int indent = 0;
        while (indent < line.length() && line[indent] == ' ') {
            indent++;
        }
        
        // Page definition (2 spaces indent)
        if (indent == 2 && line.mid(indent).contains(':')) {
            // Save previous page if exists
            if (inPage && !currentPage.name.isEmpty()) {
                if (inButton && !currentButtonSection.isEmpty()) {
                    parseButton(currentPage, currentButtonSection);
                }
                m_pages.append(currentPage);
            }
            
            // Start new page
            currentPage = YamlPage();
            currentButtonSection = "";
            inPage = true;
            inButton = false;
            
            // Extract page name
            QString lineContent = line.mid(indent);
            int colonPos = lineContent.indexOf(':');
            if (colonPos > 0) {
                currentPage.name = lineContent.left(colonPos).trimmed();
            }
        }
        // Button definition (6 spaces indent followed by - name:)
        // In YAML format: page (2sp) -> buttons (4sp) -> - name: (6sp)
        else if (inPage && (indent == 6 || indent == 4)) {
            QString trimmed = line.mid(indent).trimmed();
            // Check for button_ format OR list item format (- name:)
            if (trimmed.startsWith("button_") || 
                (trimmed[0].isDigit() && trimmed.contains(':')) ||
                trimmed.startsWith("- name:") ||
                trimmed.startsWith("-")) {
                // Save previous button
                if (inButton && !currentButtonSection.isEmpty()) {
                    parseButton(currentPage, currentButtonSection);
                }
                
                // Start new button
                inButton = true;
                currentButtonSection = trimmed + "\n";
                buttonIndentLevel = indent;
            } else if (inButton && indent >= buttonIndentLevel) {
                currentButtonSection += line + "\n";
            }
        }
        // Content within button (8+ spaces indent or deeper than button)
        else if (inPage && inButton && indent > buttonIndentLevel) {
            currentButtonSection += line + "\n";
        }
        // End of pages section (less indent)
        else if (indent < 2 && !line.mid(indent).isEmpty() && !line.mid(indent).startsWith('#')) {
            break;
        }
    }
    
    // Don't forget last button and page
    if (inPage && !currentPage.name.isEmpty()) {
        if (inButton && !currentButtonSection.isEmpty()) {
            parseButton(currentPage, currentButtonSection);
        }
        m_pages.append(currentPage);
    }
}

QStringList YamlParser::pageNames() const
{
    QStringList names;
    for (const auto &page : m_pages) {
        names.append(page.name);
    }
    return names;
}

YamlPage YamlParser::page(const QString &name) const
{
    for (const auto &p : m_pages) {
        if (p.name == name) return p;
    }
    return YamlPage();
}

void YamlParser::parseButton(YamlPage &page, const QString &buttonContent)
{
    YamlButton btn;
    
    // Extract name
    QRegularExpression nameRe("name:\\s*\"?([^\"\\n]+)\"?");
    QRegularExpressionMatch match = nameRe.match(buttonContent);
    if (match.hasMatch()) {
        btn.name = match.captured(1).trimmed();
    }
    
    // Extract icon
    QRegularExpression iconRe("icon:\\s*(\\S+)");
    match = iconRe.match(buttonContent);
    if (match.hasMatch()) {
        btn.icon = match.captured(1).trimmed();
    }
    
    // Extract entity_id
    QRegularExpression entityRe("entity_id:\\s*(\\S+)");
    match = entityRe.match(buttonContent);
    if (match.hasMatch()) {
        btn.entityId = match.captured(1).trimmed();
    }
    
    // Extract presets - handle both formats:
    // 1. presets: [preset1, preset2]
    // 2. presets:\n      - preset1\n      - preset2
    QRegularExpression presetsListRe("presets:\\s*\\[([^\\]]+)\\]");
    QRegularExpressionMatch presetsMatch = presetsListRe.match(buttonContent);
    if (presetsMatch.hasMatch()) {
        QString presetsStr = presetsMatch.captured(1);
        // Split by comma and clean up
        QStringList presets = presetsStr.split(',');
        for (QString preset : presets) {
            preset = preset.trimmed();
            // Remove quotes if present
            if (preset.startsWith('"') && preset.endsWith('"')) {
                preset = preset.mid(1, preset.length() - 2);
            }
            if (!preset.isEmpty()) {
                btn.presets.append(preset);
            }
        }
    } else {
        // Try alternate format with list items
        QRegularExpression presetsBlockRe("presets:\\s*\\n(?:(?:\\s*)-(?:\\s*)([^\\n]+)\\n?)+");
        QRegularExpressionMatch blockMatch = presetsBlockRe.match(buttonContent);
        if (blockMatch.hasMatch()) {
            QRegularExpression itemRe("\\s*-(?:\\s*)([^\\n]+)");
            QRegularExpressionMatchIterator itemIter = itemRe.globalMatch(blockMatch.captured(0));
            while (itemIter.hasNext()) {
                QRegularExpressionMatch itemMatch = itemIter.next();
                QString preset = itemMatch.captured(1).trimmed();
                // Remove quotes if present
                if (preset.startsWith('"') && preset.endsWith('"')) {
                    preset = preset.mid(1, preset.length() - 2);
                }
                if (!preset.isEmpty()) {
                    btn.presets.append(preset);
                }
            }
        }
    }
    
    // Parse states section if present
    parseStates(buttonContent, btn);
    
    // If no icon found, try to extract from states (fallback for old configs)
    if (btn.icon.isEmpty() && !btn.states.isEmpty()) {
        // Try to get icon from first state (usually "on")
        for (auto it = btn.states.begin(); it != btn.states.end(); ++it) {
            if (!it.value().icon.isEmpty()) {
                btn.icon = it.value().icon;
                break;
            }
        }
    }
    
    // Extract tap_action - handle multiple formats:
    // 1. action: $page.go_to with data: page_name
    // 2. action: $cmd.exec with data: cmd: "..."
    // 3. entity_id with tap_action (lights, switches, etc.)
    
    // First try to extract tap_action directly
    QRegularExpression tapActionRe("tap_action:[\\s\\S]*?action:\\s*(\\S+)");
    QRegularExpressionMatch tapMatch = tapActionRe.match(buttonContent);
    if (tapMatch.hasMatch()) {
        btn.tapAction = tapMatch.captured(1).trimmed();
        
        // For $page.go_to, extract the page name from data
        if (btn.tapAction == "$page.go_to") {
            QRegularExpression dataRe("data:\\s*(.+?)(?:\\n|\\r|$)");
            QRegularExpressionMatch dataMatch = dataRe.match(buttonContent);
            if (dataMatch.hasMatch()) {
                QString entityId = dataMatch.captured(1).trimmed();
                // Remove quotes if present
                if ((entityId.startsWith('"') && entityId.endsWith('"')) ||
                    (entityId.startsWith("'") && entityId.endsWith("'"))) {
                    entityId = entityId.mid(1, entityId.length() - 2);
                }
                btn.entityId = entityId;
            }
        }
        // For $cmd.exec or $cmd.exec_text, extract the cmd from data
        else if (btn.tapAction.startsWith("$cmd.")) {
            QRegularExpression cmdRe("cmd:\\s*\"?([^\"\\n]+)\"?");
            QRegularExpressionMatch cmdMatch = cmdRe.match(buttonContent);
            if (cmdMatch.hasMatch()) {
                QString cmd = cmdMatch.captured(1).trimmed();
                // Remove trailing quote if present
                if (cmd.endsWith('"')) {
                    cmd = cmd.left(cmd.length() - 1);
                }
                btn.entityId = cmd;
            }
        }
    }
    
    // If no tap_action found, fall back to entity_id method
    if (btn.tapAction.isEmpty()) {
        QRegularExpression entityRe("entity_id:\\s*(.+?)(?:\\n|\\r|$)");
        match = entityRe.match(buttonContent);
        if (match.hasMatch()) {
            QString entityId = match.captured(1).trimmed();
            // Remove quotes if present
            if ((entityId.startsWith('"') && entityId.endsWith('"')) ||
                (entityId.startsWith("'") && entityId.endsWith("'"))) {
                entityId = entityId.mid(1, entityId.length() - 2);
            }
            btn.entityId = entityId;
            // Try to determine action type from tap_action
            QRegularExpression actionRe("tap_action:[\\s\\S]*?action:\\s*(\\S+)");
            QRegularExpressionMatch actionMatch = actionRe.match(buttonContent);
            if (actionMatch.hasMatch()) {
                btn.tapAction = actionMatch.captured(1).trimmed();
            }
            
            // If it's a light and no icon found, use default lightbulb icon
            if (btn.icon.isEmpty() && btn.entityId.startsWith("light.")) {
                btn.icon = "mdi:lightbulb";
            }
        }
    }
    
    page.buttons.append(btn);
}

void YamlParser::parseStates(const QString &buttonContent, YamlButton &btn)
{
    // Find states: section
    int statesPos = buttonContent.indexOf("states:");
    if (statesPos == -1) {
        return;
    }
    
    // Extract the states section content
    QString statesSection = buttonContent.mid(statesPos + 7); // Skip "states:"
    
    // Find end of states section (next top-level key in button or end)
    int statesEnd = statesSection.indexOf(QRegularExpression("\n[a-zA-Z_][a-zA-Z0-9_]*:"));
    if (statesEnd == -1) {
        statesEnd = statesSection.length();
    }
    statesSection = statesSection.left(statesEnd);
    
    // Parse individual states (indented by 4 spaces relative to states:)
    // Format:
    //   states:
    //     on:
    //       icon: mdi:lightbulb
    //       name: "Light On"
    //     off:
    //       icon: mdi:lightbulb-off
    
    QRegularExpression stateRe("\\n    ([a-zA-Z_][a-zA-Z0-9_]*):");
    QRegularExpressionMatchIterator stateIter = stateRe.globalMatch(statesSection);
    
    while (stateIter.hasNext()) {
        QRegularExpressionMatch stateMatch = stateIter.next();
        QString stateKey = stateMatch.captured(1);
        
        // Extract the content for this state
        int stateStart = stateMatch.capturedStart();
        int stateEnd = statesSection.indexOf(QRegularExpression("\\n    [a-zA-Z_][a-zA-Z0-9_]*:"), stateStart + 1);
        if (stateEnd == -1) {
            stateEnd = statesSection.length();
        }
        QString stateContent = statesSection.mid(stateStart, stateEnd - stateStart);
        
        StateConfig config;
        config.stateValue = stateKey;
        
        // Extract icon
        QRegularExpression iconRe("icon:\\s*(\\S+)");
        QRegularExpressionMatch iconMatch = iconRe.match(stateContent);
        if (iconMatch.hasMatch()) {
            config.icon = iconMatch.captured(1).trimmed();
        }
        
        // Extract name
        QRegularExpression nameRe("name:\\s*\"?([^\"\\n]+)\"?");
        QRegularExpressionMatch nameMatch = nameRe.match(stateContent);
        if (nameMatch.hasMatch()) {
            config.name = nameMatch.captured(1).trimmed();
        }
        
        // Extract preset
        QRegularExpression presetRe("preset:\\s*(\\S+)");
        QRegularExpressionMatch presetMatch = presetRe.match(stateContent);
        if (presetMatch.hasMatch()) {
            config.presets.append(presetMatch.captured(1).trimmed());
        }
        
        // Extract presets list (if multiple)
        QRegularExpression presetsListRe("presets:\\s*\\[([^\\]]+)\\]");
        QRegularExpressionMatch presetsListMatch = presetsListRe.match(stateContent);
        if (presetsListMatch.hasMatch()) {
            QString presetsStr = presetsListMatch.captured(1);
            QStringList presets = presetsStr.split(',');
            for (QString preset : presets) {
                preset = preset.trimmed();
                if (preset.startsWith('"') && preset.endsWith('"')) {
                    preset = preset.mid(1, preset.length() - 2);
                }
                if (!preset.isEmpty()) {
                    config.presets.append(preset);
                }
            }
        }
        
        // Extract text
        QRegularExpression textRe("text:\\s*\"?([^\"\\n]+)\"?");
        QRegularExpressionMatch textMatch = textRe.match(stateContent);
        if (textMatch.hasMatch()) {
            config.text = textMatch.captured(1).trimmed();
        }
        
        btn.states[stateKey] = config;
    }
}

bool YamlParser::saveToFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream << content;
    file.close();
    
    return true;
}

QString YamlParser::generateButtonYaml(const YamlButton &button, int indent) const
{
    QString spaces(indent, ' ');
    QString yaml;
    
    yaml += spaces + "- name: \"" + button.name + "\"\n";
    
    if (!button.icon.isEmpty()) {
        yaml += spaces + "  icon: " + button.icon + "\n";
    }
    
    if (!button.tapAction.isEmpty()) {
        if (button.tapAction == "$page.go_to") {
            yaml += spaces + "  tap_action:\n";
            yaml += spaces + "    action: " + button.tapAction + "\n";
            yaml += spaces + "    data: " + button.entityId + "\n";
        } else if (button.tapAction.startsWith("$cmd.")) {
            yaml += spaces + "  tap_action:\n";
            yaml += spaces + "    action: " + button.tapAction + "\n";
            yaml += spaces + "    data:\n";
            yaml += spaces + "      cmd: \"" + button.entityId + "\"\n";
        } else if (!button.entityId.isEmpty()) {
            yaml += spaces + "  entity_id: " + button.entityId + "\n";
            yaml += spaces + "  tap_action:\n";
            yaml += spaces + "    action: " + button.tapAction + "\n";
        }
    }
    
    return yaml;
}

QString YamlParser::updateButtonInYaml(const QString &content, const QString &pageName, int buttonIndex, const YamlButton &newButton)
{
    // Find the page section
    QString pageSearch = pageName + ":";
    int pagePos = content.indexOf(pageSearch);
    if (pagePos == -1) {
        qDebug() << "[YamlParser] Page not found:" << pageName;
        return content;
    }
    
    // Find buttons section within page
    int buttonsPos = content.indexOf("buttons:", pagePos);
    if (buttonsPos == -1) {
        qDebug() << "[YamlParser] Buttons section not found in page:" << pageName;
        return content;
    }
    
    // Find the specific button by index
    int currentButton = 1;
    int buttonPos = content.indexOf("- name:", buttonsPos);
    
    while (buttonPos != -1 && currentButton < buttonIndex) {
        buttonPos = content.indexOf("- name:", buttonPos + 1);
        currentButton++;
    }
    
    if (buttonPos == -1 || currentButton != buttonIndex) {
        qDebug() << "[YamlParser] Button not found:" << buttonIndex;
        return content;
    }
    
    // Find end of this button (next button or end of buttons section)
    int nextButtonPos = content.indexOf("- name:", buttonPos + 1);
    int pageEndPos = content.indexOf(QRegularExpression("\n  [a-zA-Z_$][a-zA-Z0-9_]*:"), pagePos + 1);
    
    int buttonEndPos;
    if (nextButtonPos != -1 && (pageEndPos == -1 || nextButtonPos < pageEndPos)) {
        buttonEndPos = nextButtonPos;
    } else if (pageEndPos != -1) {
        buttonEndPos = pageEndPos;
    } else {
        buttonEndPos = content.length();
    }
    
    // Generate new button YAML
    QString newButtonYaml = generateButtonYaml(newButton);
    
    // Replace old button with new one
    QString result = content.left(buttonPos) + newButtonYaml + content.mid(buttonEndPos);
    
    qDebug() << "[YamlParser] Updated button" << buttonIndex << "in page" << pageName;
    return result;
}

QString YamlParser::deleteButtonFromYaml(const QString &content, const QString &pageName, int buttonIndex)
{
    // Find the page section
    QString pageSearch = pageName + ":";
    int pagePos = content.indexOf(pageSearch);
    if (pagePos == -1) {
        qDebug() << "[YamlParser] Page not found:" << pageName;
        return content;
    }
    
    // Find buttons section within page
    int buttonsPos = content.indexOf("buttons:", pagePos);
    if (buttonsPos == -1) {
        qDebug() << "[YamlParser] Buttons section not found in page:" << pageName;
        return content;
    }
    
    // Find the specific button by index
    int currentButton = 1;
    int buttonPos = content.indexOf("- name:", buttonsPos);
    
    while (buttonPos != -1 && currentButton < buttonIndex) {
        buttonPos = content.indexOf("- name:", buttonPos + 1);
        currentButton++;
    }
    
    if (buttonPos == -1 || currentButton != buttonIndex) {
        qDebug() << "[YamlParser] Button not found:" << buttonIndex;
        return content;
    }
    
    // Find end of this button (next button or end of buttons section)
    int nextButtonPos = content.indexOf("- name:", buttonPos + 1);
    int pageEndPos = content.indexOf(QRegularExpression("\n  [a-zA-Z_$][a-zA-Z0-9_]*:"), pagePos + 1);
    
    int buttonEndPos;
    if (nextButtonPos != -1 && (pageEndPos == -1 || nextButtonPos < pageEndPos)) {
        buttonEndPos = nextButtonPos;
    } else if (pageEndPos != -1) {
        buttonEndPos = pageEndPos;
    } else {
        buttonEndPos = content.length();
    }
    
    // Remove this button
    QString result = content.left(buttonPos) + content.mid(buttonEndPos);
    
    qDebug() << "[YamlParser] Deleted button" << buttonIndex << "from page" << pageName;
    return result;
}

QString YamlParser::deletePageFromYaml(const QString &content, const QString &pageName)
{
    // Find the page section
    QString pageSearch = pageName + ":";
    int pagePos = content.indexOf(pageSearch);
    if (pagePos == -1) {
        qDebug() << "[YamlParser] Page not found:" << pageName;
        return content;
    }
    
    // Find start of this page (including preceding whitespace)
    int pageStartPos = pagePos;
    while (pageStartPos > 0 && content[pageStartPos - 1] != '\n') {
        pageStartPos--;
    }
    
    // Find end of this page (next page at same indent level or end of file)
    int nextPagePos = content.indexOf(QRegularExpression("\n  [a-zA-Z_$][a-zA-Z0-9_]*:"), pagePos + 1);
    
    int pageEndPos;
    if (nextPagePos != -1) {
        pageEndPos = nextPagePos + 1; // Include the newline
    } else {
        pageEndPos = content.length();
    }
    
    // Remove this page
    QString result = content.left(pageStartPos) + content.mid(pageEndPos);
    
    qDebug() << "[YamlParser] Deleted page:" << pageName;
    return result;
}

QString YamlParser::addPageToYaml(const QString &content, const QString &pageName, const QString &wallpaperPath, const QString &layoutType)
{
    // Check if page already exists
    QString pageSearch = pageName + ":";
    if (content.contains(pageSearch)) {
        qDebug() << "[YamlParser] Page already exists:" << pageName;
        return content;
    }
    
    // Find the end of the pages section (after last page)
    // Look for the pattern: pages: followed by indented pages
    int pagesPos = content.indexOf("pages:");
    if (pagesPos == -1) {
        qDebug() << "[YamlParser] No pages section found";
        return content;
    }
    
    // Find the last page (look for the last entry at indent level 2: "  page_name:")
    QRegularExpression pageRegex("\n  ([a-zA-Z_$][a-zA-Z0-9_]*):");
    QRegularExpressionMatchIterator matchIter = pageRegex.globalMatch(content);
    
    int lastPageEnd = pagesPos + 6; // After "pages:"
    int lastMatchEnd = -1;
    
    while (matchIter.hasNext()) {
        QRegularExpressionMatch match = matchIter.next();
        lastMatchEnd = match.capturedEnd();
    }
    
    if (lastMatchEnd != -1) {
        // Find the end of that page (next page at indent level 2, or end of pages section)
        QRegularExpression nextPageRegex("\n  ([a-zA-Z_$][a-zA-Z0-9_]*):");
        QString remaining = content.mid(lastMatchEnd);
        QRegularExpressionMatch nextMatch = nextPageRegex.match(remaining);
        
        if (nextMatch.hasMatch()) {
            lastPageEnd = lastMatchEnd + nextMatch.capturedStart();
        } else {
            // This is the last page, find where pages section ends
            // Look for next top-level section (unindented)
            QRegularExpression nextSectionRegex("\n([a-zA-Z_][a-zA-Z0-9_]*):");
            QString afterPages = content.mid(pagesPos + 6);
            QRegularExpressionMatch sectionMatch = nextSectionRegex.match(afterPages);
            
            if (sectionMatch.hasMatch()) {
                lastPageEnd = pagesPos + 6 + sectionMatch.capturedStart();
            } else {
                lastPageEnd = content.length();
            }
        }
    } else {
        // No existing pages, insert right after "pages:"
        lastPageEnd = pagesPos + 6;
    }
    
    // Generate new page YAML
    QString newPageYaml = QString("\n  %1:\n    wallpaper:\n      path: \"%2\"\n    buttons:\n").arg(pageName).arg(wallpaperPath.isEmpty() ? "" : wallpaperPath);
    
    // Add sample buttons based on layout
    if (layoutType == "1x1") {
        newPageYaml += QString("      - name: \"Button 1\"\n        icon: mdi:flash\n        tap_action:\n          action: $page.go_to\n          data: $root\n");
    } else {
        // 3x5 standard layout - add some sample buttons
        newPageYaml += QString("      - name: \"Home\"\n        icon: mdi:home\n        tap_action:\n          action: $page.go_to\n          data: $root\n");
    }
    
    // Insert new page at the calculated position
    QString result = content.left(lastPageEnd) + newPageYaml + content.mid(lastPageEnd);
    
    qDebug() << "[YamlParser] Added page:" << pageName;
    return result;
}
