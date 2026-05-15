#ifndef YAMLHIGHLIGHTER_H
#define YAMLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>

class YamlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit YamlHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    QVector<HighlightingRule> highlightingRules;
    
    QTextCharFormat keyFormat;
    QTextCharFormat valueFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat variableFormat;
    QTextCharFormat actionFormat;
    QTextCharFormat iconFormat;
};

#endif // YAMLHIGHLIGHTER_H
