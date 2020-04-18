#include "xmledit.h"
#include <QMessageBox>
#include <QTextStream>
#include <QStack>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

uint64_t strToMs(const QString &s, bool *success) {
	*success = false;
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

	*success = true;
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
	standaloneKeys["GameName"] = tr("Game name:");
	standaloneKeys["CategoryName"] = tr("Category name:");
	standaloneKeys["AttemptCount"] = tr("Attempts");
	standaloneKeys["Offset"] = tr("Offset:");

	runTableLabels += QString(tr("Split name", "Table header split name"));
	runTableLabels += QString(tr("Split", "Table header split time"));
	runTableLabels += QString(tr("Total", "Table header total time"));
}

XmlEdit::~XmlEdit() {
	
}

void XmlEdit::clearUi() {
	DocumentEdit::clearUi();

	vLayout = new QVBoxLayout(widget());
	widget()->setLayout(vLayout);
}

// Unused, artifact of old XmlEdit program
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
	messageBox.setText(QString(tr("Could not open this file: %1").arg(message)));
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

qint64 XmlEdit::fetchId(ParseState &state, QDomElement element) {
	bool tempSuccess;
	qint64 id = fetchElementInt(element, "id", &tempSuccess);
	if (!tempSuccess) {
		addNodeFail(state, QString(tr("Couldn't understand attempt id: \"%1\"")).arg(fetchElement(element, "id")));
		return 0;
	}
	return id;
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
						} else if (tag == "Segments") {
							state.kind = PARSING_SEGMENT_SCAN;
							topSegment = -1;
						} else if (standaloneKeys.count(tag)) {
							state.kind = PARSING_STANDALONE;
							state.str1 = standaloneKeys[tag];
						}
					} break;
				case PARSING_ATTEMPT_SCAN: {
					if (tag == "Attempt") { // We have found an attempt, set it up in run keys
						qint64 id = fetchId(state, element);
						if (state.dead) return;

						runKeys.append(id);
						SingleRun &run = runs[id];
						run.timeLabel = fetchElement(element, "started");
						state.kind = PARSING_ATTEMPT_INSIDE;
						state.int1 = id;
					}
				} break;
				case PARSING_ATTEMPT_INSIDE: // In <Attempt> looking for <AttemptHistory>
					if (tag == "AttemptHistory") {
						state.kind = PARSING_ATTEMPT_REALTIME;
					} break;
				case PARSING_SEGMENT_SCAN: // In <Segments> looking for <Segment>
					if (tag == "Segment") {
						topSegment++;
						state.kind = PARSING_SEGMENT;
					} break;
				case PARSING_SEGMENT: { // In <Segment> looking for one of several things
					if (tag == "Name") {
						state.kind = PARSING_SEGMENT_NAME;
					} else if (tag == "SplitTimes") {
						state.kind = PARSING_SEGMENT_PB_SPLITTIMES;
					} else if (tag == "BestSegmentTime") {
						state.kind = PARSING_SEGMENT_BESTSPLIT_BESTSEGMENTTIME;
					} else if (tag == "SegmentHistory") {
						state.kind = PARSING_SEGMENT_HISTORY;
					}
				} break;
			    case PARSING_SEGMENT_PB_SPLITTIMES: // In <Segment><SplitTimes> looking for <SplitTime>
					if (tag == "SplitTime") {
						state.kind = PARSING_SEGMENT_PB_SPLITTIME;
					} break;
				case PARSING_SEGMENT_PB_SPLITTIME: // In <Segment><SplitTimes><SplitTime> looking for <RealTime>
					if (tag == "RealTime") {
						state.kind = PARSING_SEGMENT_BESTSPLIT_REALTIME;
					} break;
		        case PARSING_SEGMENT_BESTSPLIT_BESTSEGMENTTIME: // In <Segment><BestSegmentTime> looking for <RealTime>
		        	if (tag == "RealTime") {
						state.kind = PARSING_SEGMENT_PB_REALTIME;
					} break;
		        case PARSING_SEGMENT_HISTORY: // In <SegmentHistory> looking for <Time>
		        	if (tag == "Time") { // We have now found data from an actual run
						qint64 id = fetchId(state, element); // Run id
						if (state.dead) return;

						// Create data structure for run
						SingleRun &run = runs[id];
						Q_ASSERT_X(topSegment >= 0, "XML parse", "topSegment is uninitialized");
						while (run.splits.size() <= topSegment) // Run ID is specified explicitly but split ID is implicit by order
							run.splits.push_back(SingleSplit());
						SingleSplit &split = run.splits[topSegment];
						split.timeXml = element; // Need to save this if deletion is needed later

						state.kind = PARSING_SEGMENT_HISTORY_RUN;
						state.int1 = id;
					} break;
    			case PARSING_SEGMENT_HISTORY_RUN: // In <SegmentHistory><Time> looking for <RealTime>
		        	if (tag == "RealTime") {
						SingleRun &run = runs[state.int1];
						SingleSplit &split = run.splits[topSegment];
						split.realTimeXml = element; // Need to save this if deletion is needed later

						state.kind = PARSING_SEGMENT_HISTORY_RUN_REALTIME;
					} break;
				default:break;
			}
		} break;
		case QDomNode::TextNode: {
			QDomCharacterData text = node.toCharacterData();

			switch(state.kind) {
				case PARSING_STANDALONE: { // One of the XML parameters that's in a standalone edit box at the top
					// The standalone boxes are the only ones we layout in this initial XML-parsing pass
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
				case PARSING_ATTEMPT_REALTIME: { // Found the "total time" for a run, save position to edit later
					SingleRun &run = runs[state.int1];
					run.realTimeTotal = text;
				} break;
				case PARSING_SEGMENT_NAME: { // Found a segment name
					while (splitNames.size() < topSegment)
						splitNames.append(QString());
					splitNames.append(text.data());
				} break;
				case PARSING_SEGMENT_PB_REALTIME: // Found a split time
				case PARSING_SEGMENT_BESTSPLIT_REALTIME:
				case PARSING_SEGMENT_HISTORY_RUN_REALTIME: {
					bool success;
					uint64_t time = strToMs(text.data(), &success);
					if (!success) {
						addNodeFail(state, QString(tr("Couldn't parse time: \"%1\"")).arg(text.data()));
						break;
					}
					switch(state.kind) {
						case PARSING_SEGMENT_HISTORY_RUN_REALTIME: { // It's from a run
							SingleRun &run = runs[state.int1];
							SingleSplit &split = run.splits[topSegment];
							split.textXml = text;
							split.splitHas = true;
							split.splitMs = time;
						} break;
						default: { // Debug, remove me
							printf("kind %d, content %s\n", state.kind, text.data().toStdString().c_str());
							printf("time %lld\n", time);
							QString s = msToStr(time);
							printf("reverse time %s\n", s.toStdString().c_str());
						} break;
					}
				}
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

void XmlEdit::renderRun(QString runLabel, SingleRun &run, QWidget *content, QVBoxLayout *vContentLayout) {
	{
		QFrame *line = new QFrame(content); // Magic <hr> code from Stack Overflow
		line->setObjectName(QString::fromUtf8("line"));
		line->setGeometry(QRect(320, 150, 118, 3));
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		vContentLayout->addWidget(line);
	}

	{ // Label is an hbox so there can be a "PB" badge later
		QWidget *labelHbox = new QWidget(content);
		QHBoxLayout *labelHLayout = new QHBoxLayout(labelHbox);
		labelHLayout->setContentsMargins(0,0,0,0);
		QLabel *label = new QLabel(labelHbox);
		label->setText(runLabel);
		labelHLayout->addWidget(label);
		vContentLayout->addWidget(labelHbox);
	}

	if (run.splits.size()) { // Split table (if any)
		QTableWidget *table = new QTableWidget(run.splits.size(), 3, content);
		table->setHorizontalHeaderLabels(runTableLabels);

    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
    		SingleSplit &split = run.splits[sidx];

    		QTableWidgetItem *splitTitle = new QTableWidgetItem();
    		if (sidx < splitNames.size())
    			splitTitle->setText(splitNames[sidx]);
    		splitTitle->setFlags(0);
    		table->setItem(sidx, 0, splitTitle);

    		QTableWidgetItem *splitTime = new QTableWidgetItem();
    		if (split.splitHas)
    			splitTime->setText(msToStr(split.splitMs));
    		table->setItem(sidx, 1, splitTime);
    		split.splitTimeWidget = splitTime;

    		QTableWidgetItem *totalTime = new QTableWidgetItem();
    		if (split.totalHas)
    			totalTime->setText(msToStr(split.totalMs));
    		table->setItem(sidx, 2, totalTime);
    		split.totalTimeWidget = totalTime;
    	}

    	vContentLayout->addWidget(table);
    	run.tableWidget = table;
    }
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

    // Parse XML
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

    // Build tables
    for(int ridx = 0; ridx < runKeys.size(); ridx++) {
    	int id = runKeys[ridx];
    	SingleRun &run = runs[id];

    	renderRun(QString("Run %1: %2").arg(id).arg(run.timeLabel), run, content, vContentLayout);
    }

    // Double-check tables
    for(int ridx = 0; ridx < runKeys.size(); ridx++) {
    	int id = runKeys[ridx];
    	SingleRun &run = runs[id];
    	if (run.tableWidget) {
    		uint64_t total = 0;
	    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
	    		SingleSplit &split = run.splits[sidx];
	    		if (split.splitHas) {
	    			total += split.splitMs;
	    			if (split.totalTimeWidget)
	    				split.totalTimeWidget->setText(msToStr(total));
	    		}
	    	}
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

