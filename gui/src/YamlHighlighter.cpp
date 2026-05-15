#include "YamlHighlighter.h"
#include <QTextCharFormat>
#include <QColor>

YamlHighlighter::YamlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Keys (left side of colon)
    keyFormat.setForeground(QColor("#569CD6"));  // Light blue
    keyFormat.setFontWeight(QFont::Bold);
    
    HighlightingRule rule;
    rule.pattern = QRegularExpression("^\\s*([a-zA-Z_][a-zA-Z0-9_]*):");
    rule.format = keyFormat;
    highlightingRules.append(rule);
    
    // String values (quoted)
    stringFormat.setForeground(QColor("#CE9178"));  // Light orange/red
    rule.pattern = QRegularExpression("\"[^\"]*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);
    
    // Numbers
    numberFormat.setForeground(QColor("#B5CEA8"));  // Light green
    rule.pattern = QRegularExpression("\\b\\d+\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);
    
    // Comments
    commentFormat.setForeground(QColor("#6A9955"));  // Green
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("#.*$");
    rule.format = commentFormat;
    highlightingRules.append(rule);
    
    // Variables ($variable)
    variableFormat.setForeground(QColor("#9CDCFE"));  // Cyan
    rule.pattern = QRegularExpression("\\$[a-zA-Z_][a-zA-Z0-9_]*");
    rule.format = variableFormat;
    highlightingRules.append(rule);
    
    // Action types
    actionFormat.setForeground(QColor("#C586C0"));  // Purple
    rule.pattern = QRegularExpression("\\$(?:cmd|page|service)\\.[a-zA-Z_]+|script\\.[a-zA-Z_]+|light\\.[a-zA-Z_]+|switch\\.[a-zA-Z_]+|button\\.[a-zA-Z_]+");
    rule.format = actionFormat;
    highlightingRules.append(rule);
    
    // MDI icons
    iconFormat.setForeground(QColor("#4EC9B0"));  // Teal
    rule.pattern = QRegularExpression("mdi:[a-zA-Z0-9-_]+");
    rule.format = iconFormat;
    highlightingRules.append(rule);
    
    // Booleans
    QTextCharFormat boolFormat;
    boolFormat.setForeground(QColor("#569CD6"));  // Blue
    rule.pattern = QRegularExpression("\\b(?:true|false|yes|no|on|off)\\b");
    rule.format = boolFormat;
    highlightingRules.append(rule);
    
    // Entity IDs (light.xxx, switch.xxx, etc.)
    QTextCharFormat entityFormat;
    entityFormat.setForeground(QColor("#DCDCAA"));  // Yellow
    rule.pattern = QRegularExpression("\\b(?:light|switch|script|sensor|binary_sensor|automation|scene|input_boolean|input_select)\\.[a-zA-Z0-9_]+\\b");
    rule.format = entityFormat;
    highlightingRules.append(rule);
}

void YamlHighlighter::highlightBlock(const QString &text)
{
    // Apply rules
    for (const HighlightingRule &rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // Special handling for YAML keys at start of line
    QRegularExpression keyRegex("^\\s*([a-zA-Z_][a-zA-Z0-9_]*):");
    QRegularExpressionMatch keyMatch = keyRegex.match(text);
    if (keyMatch.hasMatch()) {
        // Highlight the key part
        setFormat(keyMatch.capturedStart(1), keyMatch.capturedLength(1), keyFormat);
    }
    
    // Special handling for list items (- name:)
    QRegularExpression listRegex("^-\\s+([a-zA-Z_][a-zA-Z0-9_]*):");
    QRegularExpressionMatch listMatch = listRegex.match(text);
    if (listMatch.hasMatch()) {
        setFormat(listMatch.capturedStart(1), listMatch.capturedLength(1), keyFormat);
    }
    
    // Handle multi-line block scalar indicators (|, >, etc.)
    QRegularExpression blockScalarRegex(":\\s*[|>]\\s*$");
    QRegularExpressionMatch blockMatch = blockScalarRegex.match(text);
    if (blockMatch.hasMatch()) {
        // This line starts a block scalar
        setFormat(blockMatch.capturedStart(), blockMatch.capturedLength(), commentFormat);
    }
}
