#ifndef WATCHERS_H

#include <QPlainTextEdit>
#include <QDomCharacterData>
#include <QLineEdit>
#include <QDomElement>

// This file is absurdly repetitive because Qt doesn't have proper template support

class ShortCharacterDataWatcher : public QObject {
	Q_OBJECT
protected:
	QLineEdit* edit;
	QDomCharacterData data;
public:
	ShortCharacterDataWatcher(QLineEdit* _edit, const QDomCharacterData &_data) : QObject(_edit), edit(_edit), data(_data) {
		connect(edit, &QLineEdit::textChanged,
            this, &ShortCharacterDataWatcher::changed);
	}
public Q_SLOTS:
	void changed() {
		data.setData(edit->text());
	}
};

class CharacterDataWatcher : public QObject {
	Q_OBJECT
protected:
	QPlainTextEdit* edit;
	QDomCharacterData data;
public:
	CharacterDataWatcher(QPlainTextEdit* _edit, const QDomCharacterData &_data) : QObject(_edit), edit(_edit), data(_data) {
		connect(edit, &QPlainTextEdit::textChanged,
            this, &CharacterDataWatcher::changed);
	}
public Q_SLOTS:
	void changed() {
		data.setData(edit->toPlainText());
	}
};

class TagNameWatcher : public QObject {
	Q_OBJECT
protected:
	QLineEdit* edit;
	QDomElement data;
public:
	TagNameWatcher(QLineEdit* _edit, const QDomElement &_data) : QObject(_edit), edit(_edit), data(_data) {
		connect(edit, &QLineEdit::textChanged,
            this, &TagNameWatcher::changed);
	}
public Q_SLOTS:
	void changed() {
		data.setTagName(edit->text());
	}
};

class AttrWatcher : public QObject {
	Q_OBJECT
protected:
	QString previousName;
	QLineEdit* nameEdit;
	QLineEdit* valueEdit;
	QDomElement data;
public:
	AttrWatcher(QLineEdit* _nameEdit, QLineEdit* _valueEdit, const QDomElement &_data) : QObject(_nameEdit), nameEdit(_nameEdit), valueEdit(_valueEdit), data(_data) {
		previousName = nameEdit->text();
		connect(nameEdit, &QLineEdit::textChanged,
            this, &AttrWatcher::nameChanged);
		connect(valueEdit, &QLineEdit::textChanged,
            this, &AttrWatcher::valueChanged);
	}
public Q_SLOTS:
	void nameChanged() {
		data.removeAttribute(previousName);
		previousName = nameEdit->text();
		data.setAttribute(previousName, valueEdit->text());
	}
	void valueChanged() {
		data.setAttribute(previousName, valueEdit->text());
	}
};

uint64_t strToMs(const QString &s, bool &success);
QString msToStr(uint64_t ms);

#endif