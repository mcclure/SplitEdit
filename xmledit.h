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
    QString timeLabel;
    QVector<SingleSplit> splits;

    QDomCharacterData realTime;

    //QDomCharacterData xml;
};

enum ParseStateKind {
    PARSING_NONE,
    PARSING_STANDALONE,
    PARSING_ATTEMPT_SCAN,
    PARSING_ATTEMPT,
    PARSING_ATTEMPT_INSIDE,
    PARSING_ATTEMPT_REALTIME,
    PARSING_SEGMENT_SCAN,
    PARSING_SEGMENT,
    PARSING_SEGMENT_NAME,
};
struct ParseState {
    ParseStateKind kind;
    QDomNode node;
    bool dead;

    QString str1; // standalone: name
    qint64 int1; // attempt:id
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

    qint64 segmentsSeen; // Initialize to -1
    SingleRun bestSplits, bestRun;
    QVector<qint64> runKeys;
    QHash<qint64, SingleRun> runs;
    QStringList splitNames;

    QHash<QString, QString> standaloneKeys;

    qint64 fetchId(ParseState &state, QDomElement element);
    void addNodeFail(ParseState &state, QString message);
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
