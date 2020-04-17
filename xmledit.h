#ifndef XMLEDIT_H
#define XMLEDIT_H

#include <QScrollArea>
#include <QDomDocument>
#include <QVBoxLayout>
#include <QVector>
#include <QHash>

// Frustratingly, Qt has no abstract document class.
// They have a text document class but it cannot be separated from its text model.
// Therefore, this class reimplements much of the interface of QPlainTextEdit and QTextDocument.
class DocumentEdit : public QScrollArea
{
    Q_OBJECT

protected:

public:
    explicit DocumentEdit(QWidget *parent = nullptr);
    virtual ~DocumentEdit() {}

    virtual bool isModified() const = 0;

public Q_SLOTS:
#ifndef QT_NO_CLIPBOARD
	virtual void cut() = 0;
    virtual void copy() = 0;
    virtual void paste() = 0;
#endif
    //virtual void undo() = 0;
    //virtual void redo() = 0;
    virtual void clear() = 0; // Also resets file state
    virtual void clearUi(); // Also resets file state
Q_SIGNALS:
	void contentsChanged();
    void copyAvailable(bool b);
    //void undoAvailable(bool);
    //void redoAvailable(bool);
    //void undoCommandAdded();
};

struct SingleSplit {
    quint64 split;
    uint64_t ms;
    QDomCharacterData outerXml;
    QDomCharacterData innerXml;
};

struct SingleRun {
    quint64 run;
    QVector<SingleSplit> splits;
    QDomCharacterData xml;
};

enum ParseStateKind {
    PARSING_NONE,
    PARSING_STANDALONE,
};
struct ParseState {
    ParseStateKind kind;
    QDomNode node;

    QString str1; // standalone name
    ParseState clone(QDomNode with) {
        ParseState result(*this);
        result.node = with;
        return result;
    }
};

class XmlEdit : public DocumentEdit
{
    Q_OBJECT

protected:
	QDomDocument domDocument; // "Model"
	QVBoxLayout *vLayout;

    SingleRun bestSplits, bestRun;
    QVector<SingleRun> runs;
    QStringList splitNames;

    QHash<QString, QString> standaloneKeys;

	void addNode(ParseState &state, int depth, QWidget *content, QVBoxLayout *vContentLayout);
    void renderRun(SingleRun &run, QWidget *content, QVBoxLayout *vContentLayout);

public:
    explicit XmlEdit(QWidget *parent = nullptr);
    virtual ~XmlEdit();

    bool isModified() const;

    bool read(QIODevice *device);
    bool write(QIODevice *device) const;

public Q_SLOTS:
#ifndef QT_NO_CLIPBOARD
	void cut();
    void copy();
    void paste();
#endif
    //void undo();
    //void redo();
    void clear(); // Also resets file state
    void clearUi(); // Also resets file state
};


#endif
