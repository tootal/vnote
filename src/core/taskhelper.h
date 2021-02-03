#ifndef TASKHELPER_H
#define TASKHELPER_H

#include <QString>
#include <QSharedPointer>


namespace vnotex {

class Notebook;

class TaskHelper
{
public:
    // helper functions
    
    static QString normalPath(const QString &p_text);
    /**
     * if p_text contain <space>
     * Wrap it in @p_chars.
     */
    static QString spaceQuote(const QString &p_text,
                              const QString &p_chars = "\"");
    
    static QStringList spaceQuote(QStringList &p_list,
                                  const QString &p_chars = "\"");
    
    static QString getCurrentFile();
    
    static QSharedPointer<Notebook> getCurrentNotebook();
    
    static QString getFileNotebookFolder(const QString p_currentFile);
    
    static QString getSelectedText();

    static QStringList getAllSpecialVariables(const QString &p_name, const QString &p_text);
    
    static QString replaceAllSepcialVariables(const QString &p_name,
                                              const QString &p_text,
                                              const QMap<QString, QString> &p_map);

    static QString evaluateJsonExpr(const QJsonObject &p_obj,
                                    const QString &p_expr);
    
    static QString getPathSeparator();
    
private:
    TaskHelper();
};

} // ns vnotex

#endif // TASKHELPER_H
