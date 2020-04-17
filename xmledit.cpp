#include "xmledit.h"
#include <QMessageBox>
#include <QTextStream>
#include <QStack>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

uint64_t strToMs(const QString &s, bool &success) {
	success = false;
	uint64_t result = 0;
	bool tempSuccess;

	QStringList csegs = s.split(":");
	QStringList decimal = csegs.constLast().split(".");

	if (decimal.size() > 2) return 0; // FAIL seconds.miliseconds is not a decimal

	result = decimal[0].toLongLong(&tempSuccess) * 1000; // Seconds
	if (!tempSuccess) return 0; // FAIL invalid seconds
	if (decimal.size() == 2) { // Allow both :0 and :0.03
		QString s = decimal[1];
		s.toLongLong(&tempSuccess); // Test valid int before u 
		if (!tempSuccess) return 0; // FAIL invalid ms
		s.truncate(3);
		result += s.toLongLong(&tempSuccess); // Milliseconds
	}

	csegs = csegs.mid(0, csegs.size()-1);
	if (csegs.size() > 0) {
		result += csegs.constLast().toLongLong(&tempSuccess)*60*1000; // Minutes
		if (!tempSuccess) return 0; // FAIL invalid minutes

		csegs = csegs.mid(0, csegs.size()-1);
		if (csegs.size() > 0) {
			result += csegs.constLast().toLongLong(&tempSuccess)*60*60*1000; // Hours
			if (!tempSuccess) return 0;  // FAIL invalid hours
			if (csegs.size() > 1) return 0; // FAIL too many colons
		}
	}

	success = true;
	return result;
}

QString msToStr(uint64_t ms) {
	uint64_t mantissa;
	QString r;

	mantissa = ms % 1000;
	ms /= 1000;
	r = QString::number(mantissa).rightJustified(3, '0');

	mantissa = ms % 60;
	ms /= 60;
	r = QString::number(mantissa).rightJustified(2, '0') + "." + r;

	mantissa = ms % 60;
	ms /= 60;
	r = QString::number(mantissa).rightJustified(2, '0') + ":" + r;

	r = QString::number(ms).rightJustified(2, '0') + ":" + r;

	return r;
}

DocumentEdit::DocumentEdit(QWidget *parent) : QScrollArea(parent) {
	setWidgetResizable(true);
}

void DocumentEdit::clearUi() {
	setWidget(new QWidget());
}

XmlEdit::XmlEdit(QWidget *parent) : DocumentEdit(parent), vLayout(NULL) {
	standaloneKeys["GameName"] = "Game name:";
	standaloneKeys["CategoryName"] = "Category name:";
	standaloneKeys["AttemptCount"] = "Attempts";
	standaloneKeys["Offset"] = "Offset:";
}

XmlEdit::~XmlEdit() {
	
}

void XmlEdit::clearUi() {
	DocumentEdit::clearUi();

	vLayout = new QVBoxLayout(widget());
	widget()->setLayout(vLayout);
}

static const QString &nodeTypeName(QDomNode::NodeType nodeType) {
	static QString Element("Element"), Attribute("Attribute"), Text("Text"),
		CDATA("CData"), EntityReference("Entity Reference"), Entity("Entity"),
		ProcessingInstruction("Processing Instruction"), Comment("Comment"),
		Document("Document"), DocumentType("Document Type"),
		DocumentFragment("DocumentFragment"), Notation("Notation"),
		Base("Untyped?"), CharacterData("CharacterData");
	switch(nodeType) {
		case QDomNode::ElementNode:               return Element;
		case QDomNode::AttributeNode:             return Attribute;
		case QDomNode::TextNode:                  return Text;
		case QDomNode::CDATASectionNode:          return CDATA;
		case QDomNode::EntityReferenceNode:       return EntityReference;
		case QDomNode::EntityNode:                return Entity;
		case QDomNode::ProcessingInstructionNode: return ProcessingInstruction;
		case QDomNode::CommentNode:               return Comment;
		case QDomNode::DocumentNode:              return Document;
		case QDomNode::DocumentTypeNode:          return DocumentType;
		case QDomNode::DocumentFragmentNode:      return DocumentFragment;
		case QDomNode::NotationNode:              return Notation;
		case QDomNode::BaseNode:                  return Base;
		case QDomNode::CharacterDataNode:         return CharacterData;
	}
} 

#include <watchers.h>

void XmlEdit::addNodeFail(ParseState &state, QString message) {
	QMessageBox messageBox(this);
	messageBox.setText(QString("Could not open this file: ") + message);
	messageBox.exec();
	state.dead = true;
}

QString fetchElement(QDomElement element, QString name) {
	QDomAttr attr = element.attributes().namedItem(name).toAttr();
	if (attr.isNull())
		return QString();
	else
		return attr.value();
}

qint64 fetchElementInt(QDomElement element, QString name, bool *success) {
	QString s = fetchElement(element, name);
	return s.toLongLong(success);
}

void XmlEdit::addNode(ParseState &state, int depth, QWidget *content, QVBoxLayout *vContentLayout) {
	QDomNode &node = state.node;

	switch(node.nodeType()) {
		case QDomNode::ElementNode: {
			QDomElement element = node.toElement();
			QString tag = element.tagName();

			switch(state.kind) {
				case PARSING_NONE: // Toplevel
					if (depth == 2) {
						if (tag == "AttemptHistory") {
							state.kind = PARSING_ATTEMPT_SCAN;
						} else if (standaloneKeys.count(tag)) {
							state.kind = PARSING_STANDALONE;
							state.str1 = standaloneKeys[tag];
						}
					} break;
				case PARSING_ATTEMPT_SCAN: {
					if (tag == "Attempt") {
						bool tempSuccess;
						qint64 id = fetchElementInt(element, "id", &tempSuccess);
						if (!tempSuccess) {
							addNodeFail(state, QString("Couldn't understand attempt id: \"%1\"").arg(fetchElement(element, "id")));
							break;
						}
					}
				} break;
				default:break;
			}
		} break;
		case QDomNode::TextNode: {
			QDomCharacterData text = node.toCharacterData();

			switch(state.kind) {
				case PARSING_STANDALONE: {
					QString label = state.str1;

					QWidget *assign = new QWidget(content);
					QHBoxLayout *hAssignLayout = new QHBoxLayout(assign);
					hAssignLayout->setContentsMargins(0,0,0,0);
					assign->setLayout(hAssignLayout);
					vContentLayout->addWidget(assign);

					QLabel *assignLabel = new QLabel(label, assign);
					hAssignLayout->addWidget(assignLabel);

					QLineEdit *assignEdit = new QLineEdit(assign);
					hAssignLayout->addWidget(assignEdit);
					//assignEdit->setFixedWidth(38*columnWidth);
					assignEdit->setText(text.data());
					new ShortCharacterDataWatcher(assignEdit, text);
				} break;
				default:break;
			}
		} break;
		case QDomNode::CommentNode:
		// These should be impossible
		case QDomNode::AttributeNode:
		case QDomNode::BaseNode:
		case QDomNode::CharacterDataNode:
		// I don't know what these are
		case QDomNode::ProcessingInstructionNode:
		case QDomNode::DocumentNode:
		case QDomNode::DocumentTypeNode:
		case QDomNode::DocumentFragmentNode:
		case QDomNode::NotationNode:
		case QDomNode::CDATASectionNode:
		case QDomNode::EntityReferenceNode:
		case QDomNode::EntityNode:
			break;
	}
}

void renderRun(SingleRun &run, QWidget *content, QVBoxLayout *vContentLayout) {
	//QLabel *kindLabel = new QLabel(content); vContentLayout->addWidget(kindLabel);
	//kindLabel->setText(nodeTypeName(node.nodeType()));
	//vContentLayout->addWidget(kindLabel);
}

bool XmlEdit::isModified() const {
	return false;
}

#ifndef QT_NO_CLIPBOARD
void XmlEdit::cut() {

}
void XmlEdit::copy() {

}
void XmlEdit::paste() {

}
#endif

bool XmlEdit::read(QIODevice *device) {
    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!domDocument.setContent(device, true, &errorStr, &errorLine,
                                &errorColumn)) {
        QMessageBox::information(window(), tr("XML Editor"),
                                 tr("Parse error at line %1, column %2:\n%3")
                                 .arg(errorLine)
                                 .arg(errorColumn)
                                 .arg(errorStr));
        return false;
    }

    clearUi();

    QDomElement root = domDocument.documentElement();
    QStack<ParseState> stack;
    ParseState current = ParseState().clone(root);

    QWidget *content = widget();
    QVBoxLayout *vContentLayout = vLayout;
    //vContentLayout->setContentsMargins(0,0,0,0);
	//content->setLayout(vContentLayout);

    while(1) {
    	if (!current.node.isNull()) {
    		// Push a copy of the state to the stack
    		stack.push(current);
    		// Allow addNode to make any state changes appropriate for this node
    		addNode(current, stack.count(), content, vContentLayout);
    		// Do we need to bail out?
    		if (current.dead) {
    			clearUi();
    			return false;
    		}
    		// Children will see the state changes, but no one else will
    		current = current.clone(current.node.firstChild());
    	} else if (!stack.count()) {
    		break;
    	} else {
    		// No children or children are finished, rewind and move to next node
    		ParseState previous = stack.pop();
    		current = previous.clone(previous.node.nextSibling());
    	}
    }

    return true;
}

bool XmlEdit::write(QIODevice *device) const {
    const int IndentSize = 4;

    QTextStream out(device);
    domDocument.save(out, IndentSize);
    return true;
}


void XmlEdit::clear() { // Also resets file state
	domDocument.clear();
	clearUi();
}

