#include "xmledit.h"
#include <QMessageBox>
#include <QTextStream>
#include <QStack>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QScrollBar>
#include <QApplication>
#include "TableWidgetNoScroll.h"

#define REALTIME_TOTAL_STR(x) (QString(tr("Total time: %1")).arg(x))
#define SUPPRESS_DEBUG_FNS

uint64_t strToUs(const QString &s, bool *success) {
	*success = false;
	uint64_t result = 0;
	bool tempSuccess;

	QStringList csegs = s.split(":");
	QStringList decimal = csegs.constLast().split(".");

	if (decimal.size() > 2) return 0; // FAIL seconds.miliseconds is not a decimal

	result = decimal[0].toLongLong(&tempSuccess) * 1000*1000; // Seconds
	if (!tempSuccess) return 0; // FAIL invalid seconds
	if (decimal.size() == 2) { // Allow both :0 and :0.03
		QString s = decimal[1];
		s.toLongLong(&tempSuccess); // Test valid int before divide
		if (!s.isEmpty() && !tempSuccess) return 0; // FAIL nonempty but invalid us
		s.truncate(6);
		while (s.size() < 6)
			s = s + "0";
		result += s.toLongLong(&tempSuccess); // Milliseconds
	}

	csegs = csegs.mid(0, csegs.size()-1);
	if (csegs.size() > 0) {
		result += csegs.constLast().toLongLong(&tempSuccess)*60*1000*1000; // Minutes
		if (!tempSuccess) return 0; // FAIL invalid minutes

		csegs = csegs.mid(0, csegs.size()-1);
		if (csegs.size() > 0) {
			result += csegs.constLast().toLongLong(&tempSuccess)*60*60*1000*1000; // Hours
			if (!tempSuccess) return 0;  // FAIL invalid hours
			if (csegs.size() > 1) return 0; // FAIL too many colons
		}
	}

	*success = true;
	return result;
}

QString usToStr(uint64_t us) {
	uint64_t mantissa;
	QString r;

	mantissa = us % (1000*1000);
	us /= (1000*1000);
	r = QString::number(mantissa).rightJustified(6, '0');

	mantissa = us % 60;
	us /= 60;
	r = QString::number(mantissa).rightJustified(2, '0') + "." + r;

	mantissa = us % 60;
	us /= 60;
	r = QString::number(mantissa).rightJustified(2, '0') + ":" + r;

	r = QString::number(us).rightJustified(2, '0') + ":" + r;

	return r;
}

#ifndef SUPPRESS_DEBUG_FNS
static void testStrToUs(QString str) {
	printf("string %s\n", str.toStdString().c_str());
	bool valid;
	uint64_t time = strToUs(str, &valid);
	if (valid) {
		printf("time %lld\n", time);
		QString s = usToStr(time);
		printf("reverse time %s\n", s.toStdString().c_str());
	} else {
		printf("time invalid\n");
	}
}
#endif

// Call textXml.setData, but create DOM nodes along the path if needed
// QDomDocument object is needed to create new nodes, so we have to pass it in :/
static void writeXml(QDomDocument domDocument, uint64_t us, bool present, QDomElement outerXml, QDomElement &realTimeXml, QDomCharacterData &textXml) {
	if (outerXml.isNull()) {
		fprintf(stderr, "Warning: Tried to modify DOM for time, but containing XML was null. This file seems to be malformed. Aborting modification\n");
		return;
	}
	if (present) {
		if (realTimeXml.isNull()) {
			realTimeXml = outerXml.insertAfter(domDocument.createElement("RealTime"), QDomNode()).toElement();
			textXml.clear();
		}
		if (textXml.isNull()) {
			textXml = realTimeXml.insertAfter(domDocument.createTextNode(QString()), QDomNode()).toCharacterData();
		}
		textXml.setData(usToStr(us));
	} else {
		if (!realTimeXml.isNull()) {
			if (!outerXml.isNull())
				outerXml.removeChild(realTimeXml);
			outerXml.clear();
		}
		textXml.clear();
	}
}

// Call writeXml for a single split
void SingleSplit::write(QDomDocument domDocument) {
	writeXml(domDocument, xmlIsTotal ? totalUs : splitUs, xmlIsTotal ? totalHas : splitHas, timeXml, realTimeXml, textXml);
}

DocumentEdit::DocumentEdit(QWidget *parent) : QScrollArea(parent) {
	setWidgetResizable(true);
}

void DocumentEdit::clearUi() {
	setWidget(new QWidget());
}

XmlEdit::XmlEdit(QWidget *parent) : DocumentEdit(parent), vLayout(NULL), stopIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserStop)), starIcon(":/images/star-48px.png"), monoFont("generic-mono-font-pqfugjdf") {
	standaloneKeys["GameName"] = tr("Game name:");
	standaloneKeys["CategoryName"] = tr("Category name:");
	standaloneKeys["AttemptCount"] = tr("Attempts");
	standaloneKeys["Offset"] = tr("Offset:");

	runTableLabels += QString(tr("Split name", "Table header split name"));
	runTableLabels += QString(tr("Split", "Table header split time"));
	runTableLabels += QString(tr("Total", "Table header total time"));

	// monoFont is intentionally assigned a nonsense name so that setStyleHint picks the font by itself
	monoFont.setStyleHint(QFont::Monospace);
	boldFont.setBold(true);
}

XmlEdit::~XmlEdit() {
	
}

void XmlEdit::clearUi() {
	DocumentEdit::clearUi();

	correctingTable = false;

	topSegment = -1; // This is all essentially UI state
	bestSplits = SingleRun();
	bestRun = SingleRun();
	runKeys.clear();
	runs.clear();
	splitNames.clear();
	columnWidthHave = false;
	bestSplitsAutomaticWidget = NULL;
	bestRunAutomaticWidget = NULL;

	vLayout = new QVBoxLayout(widget());
	widget()->setLayout(vLayout);
}

// Unused, artifact of old XmlEdit program
#ifndef SUPPRESS_DEBUG_FNS
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
#endif

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

void SingleRun::ensureSpaceFor(int splitIdx) {
	while (this->splits.size() <= splitIdx)
		this->splits.push_back(SingleSplit());
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
					if (tag == "RealTime") {
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
						// Notice: Run ID is specified explicitly but split ID is always implicit by XML order
						bestSplits.ensureSpaceFor(topSegment);
						SingleSplit &split = bestSplits.splits[topSegment];

						state.kind = PARSING_SEGMENT_BESTSPLIT_BESTSEGMENTTIME;
						split.timeXml = element;
					} else if (tag == "SegmentHistory") {
						state.kind = PARSING_SEGMENT_HISTORY;
					}
				} break;
			    case PARSING_SEGMENT_PB_SPLITTIMES: // In <SplitTimes> looking for <SplitTime name="Personal Best">
					if (tag == "SplitTime") {
						QString name = fetchElement(element, "name");
						if (name == "Personal Best") {
							bestRun.ensureSpaceFor(topSegment);
							SingleSplit &split = bestRun.splits[topSegment];

							state.kind = PARSING_SEGMENT_PB_SPLITTIME;
							split.timeXml = element;
							split.xmlIsTotal = true; // For whatever reason this is how LiveSplit measures PBs
						}
					} break;
				case PARSING_SEGMENT_PB_SPLITTIME: // In <SplitTimes><SplitTime name="Personal Best"> looking for <RealTime>
					if (tag == "RealTime") {
						SingleSplit &split = bestRun.splits[topSegment];

						state.kind = PARSING_SEGMENT_PB_REALTIME;
						split.realTimeXml = element;
					} break;
		        case PARSING_SEGMENT_BESTSPLIT_BESTSEGMENTTIME: // In <BestSegmentTime> looking for <RealTime>
		        	if (tag == "RealTime") {
		        		SingleSplit &split = bestSplits.splits[topSegment];

						state.kind = PARSING_SEGMENT_BESTSPLIT_REALTIME;
						split.realTimeXml = element;
					} break;
		        case PARSING_SEGMENT_HISTORY: // In <SegmentHistory> looking for <Time>
		        	if (tag == "Time") { // We have now found data from an actual run
						qint64 id = fetchId(state, element); // Run id
						if (state.dead) return;

						// Create data structure for run
						SingleRun &run = runs[id];
						Q_ASSERT_X(topSegment >= 0, "XML parse", "topSegment is uninitialized");
						run.ensureSpaceFor(topSegment);
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
					uint64_t time = strToUs(text.data(), &success);
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
							split.splitUs = time;
						} break;
						case PARSING_SEGMENT_PB_REALTIME: { // It's from the PB record
							SingleSplit &split = bestRun.splits[topSegment];
							split.textXml = text;
							split.totalHas = true; // Again notice PB XML is recorded as total
							split.totalUs = time;
						} break;
						case PARSING_SEGMENT_BESTSPLIT_REALTIME: { // It's a best split
							SingleSplit &split = bestSplits.splits[topSegment];
							split.textXml = text;
							split.splitHas = true;
							split.splitUs = time;
						} break;
						default: {
							Q_ASSERT_X(false, "time parse", "Unreachable code reached");
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

void XmlEdit::renderRun(QString runLabel, SingleRun &run, QWidget *content, QVBoxLayout *vContentLayout, QCheckBox **automaticBox) {
	{
		QFrame *line = new QFrame(content); // Magic <hr> code from Stack Overflow
		line->setObjectName(QString::fromUtf8("line"));
		line->setGeometry(QRect(320, 150, 118, 3));
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		vContentLayout->addWidget(line);
	}

	{ // Run labels
		QWidget *labelHbox = new QWidget(content);
		QHBoxLayout *labelHLayout = new QHBoxLayout(labelHbox);
		labelHLayout->setContentsMargins(0,0,0,0);

		QLabel *label = new QLabel(labelHbox);
		label->setText(runLabel);
		labelHLayout->addWidget(label);

		if (automaticBox) {
			*automaticBox = new QCheckBox(tr("Automatic"), labelHbox);
			labelHLayout->addWidget(*automaticBox);
		}

		QLabel *totalTime = new QLabel(labelHbox);
		QString realTimeTotalString = run.realTimeTotal.data();
		if (!realTimeTotalString.isEmpty())
			realTimeTotalString = REALTIME_TOTAL_STR(run.realTimeTotal.data());
		totalTime->setText(realTimeTotalString);
		labelHLayout->addWidget(totalTime);
		run.realTimeTotalWidget = totalTime;

		QLabel *personalBestFlag = new QLabel(labelHbox);
		personalBestFlag->setFont(boldFont);
		QPalette palette = personalBestFlag->palette();
		palette.setColor(QPalette::WindowText, QColor(255,0,0).darker());
		personalBestFlag->setPalette(palette);
		labelHLayout->addWidget(personalBestFlag);
		run.personalBestWidget = personalBestFlag;

		vContentLayout->addWidget(labelHbox);
	}

	if (run.splits.size()) { // Split table (if any)
		QTableWidget *table = new TableWidgetNoScroll(run.splits.size(), 3, content);
    	table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents); // DOES ANYTHING??
		table->setHorizontalHeaderLabels(runTableLabels);

		// This "feels wrong" but seems to be necessary...
		// Qt assumes tables always have a scrollbar, and if you want to put a table •inside* a
		// scroll area, as we are doing, and let the outer scroll area handle the scrolling, it
		// just.. won't. So we compute a fixed size for the table and disable scrolling:
		int height = table->horizontalHeader()->height() + table->horizontalHeader()->offset();
		for (int row = 0; row < table->rowCount(); ++row)
			height += table->rowHeight(row);
		table->setFixedHeight(height);
		table->verticalScrollBar()->setDisabled(true);
    	table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    	// Fill out all rows
    	bool allValid = true;
    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
    		SingleSplit &split = run.splits[sidx];
    		bool valid = split.valid();
    		allValid = allValid && valid;

    		QTableWidgetItem *splitTitle = new QTableWidgetItem();
    		if (sidx < splitNames.size())
    			splitTitle->setText(splitNames[sidx]);
    		splitTitle->setFlags(0);
    		table->setItem(sidx, 0, splitTitle);

    		QTableWidgetItem *splitTime = new QTableWidgetItem();
    		splitTime->setFont(monoFont);
    		if (!valid) { // File has been edited in split editor -- not valid
    			splitTime->setFlags(0);
    			splitTime->setText("-----");
    			splitTime->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    		} else {
    			if (split.splitHas)
    				splitTime->setText(usToStr(split.splitUs));
    			splitTime->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    		}
    		table->setItem(sidx, 1, splitTime);
    		split.splitTimeWidget = splitTime;

    		QTableWidgetItem *totalTime = new QTableWidgetItem();
    		totalTime->setFont(monoFont);
    		if (!allValid) { // Right now, if there are any invalid splits, editing a total time after this will confuse the app.
    			totalTime->setFlags(0); // So just don't let that happen.
    		}
    		if (!valid) { // File has been edited in split editor -- not valid
    			totalTime->setText("-----");
    			totalTime->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    		} else {
    			if (split.totalHas)
    				totalTime->setText(usToStr(split.totalUs));
    			totalTime->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    		}
    		table->setItem(sidx, 2, totalTime);
    		split.totalTimeWidget = totalTime;

    		// Set column sizes
    		if (!columnWidthHave) {
    			QFontMetrics nameMetrics(splitTitle->font());
    			QFontMetrics timeMetrics(totalTime->font());

    			columnWidthName = nameMetrics.horizontalAdvance("XXXXX");
    			for (int sidx = 0; sidx < splitNames.size(); sidx++) {
    				int candidateWidth = nameMetrics.horizontalAdvance(splitNames[sidx] + "XXXXX");
    				if (columnWidthName < candidateWidth)
    					columnWidthName = candidateWidth;
    			}

    			// In testing this seems to give the width of 88:88:88.888888, which is... wrong but OK?
    			columnWidthTime = timeMetrics.horizontalAdvance("8888888:88:88.888888");

    			columnWidthHave = true;
    		}
    		table->setColumnWidth(0, columnWidthName);
    		table->setColumnWidth(1, columnWidthTime);
    		table->setColumnWidth(2, columnWidthTime);
    	}

    	// Watch for changes, this routes to ::changed below
		new XmlEditTableWatcher(table, this, run);

    	vContentLayout->addWidget(table);
    	run.tableWidget = table;
    }
}

void XmlEditTableWatcher::changed(QTableWidgetItem *item) {
	// printf("DEBUG- ::CHANGED! row %d col %d correctingTable? %s data %s\n", item->row(), item->column(), xmlEdit->correctingTable?"Y":"N", item->text().toStdString().c_str());
	if (xmlEdit->correctingTable) // This slot is for catching changes by the user.
		return;          // If we set a change off ourselves, ignore it.

	// Interpret cell
	QString text = item->text();
	bool success;
	uint64_t us = strToUs(item->text(), &success);
	bool empty = text.isEmpty();

	// Reconstruct table position
	bool cellIsTotal = item->column() == 2;
	SingleSplit &split = run.splits[item->row()];

	// Make sure the clock never goes backward
	if (cellIsTotal && success && !empty) {
		int checkRow = item->row() - 1; // Check rows before
		while (checkRow >= 0) {
			SingleSplit &splitBefore = run.splits[checkRow];
			if (splitBefore.totalHas) {
				if (splitBefore.totalUs > us) {
					success = false; // Clause below will set error icon
				}
				break;
			}
			checkRow--;
		}
		if (success) {
			checkRow = item->row() + 1;
			while (checkRow < run.splits.size()) {
				SingleSplit &splitAfter = run.splits[checkRow];
				if (splitAfter.totalHas) {
					if (splitAfter.totalUs < us) {
						success = false; // Clause below will set error icon
					}
					break;
				}
				checkRow++;
			}
		}
	}

	// Note an empty input is a valid input, it implies the split was skipped
	if (success || empty) {
		// Clear error icon
		split.hasError = false;

		// Copy us value back into split
		if (cellIsTotal) {
			split.totalHas = !empty;
			split.totalUs = us;
		} else {
			split.splitHas = !empty;
			split.splitUs = us;
		}
		// Set underlying DOM element (if any)
		if (cellIsTotal == split.xmlIsTotal)
			split.write(xmlEdit->domDocument);
		// Whichever column we just changed, correct the other side
		xmlEdit->correctTable(run, cellIsTotal, true);

		// Edited last row, change total time also
		if (cellIsTotal && item->row() == (run.splits.size()-1) && run.splits.size() == xmlEdit->runTableLabels.size()) {
    		if (!run.realTimeTotal.isNull())
	    		run.realTimeTotal.setData(empty ? QString() : usToStr(us));
    		if (run.realTimeTotalWidget)
	    		run.realTimeTotalWidget->setText(empty ? QString() : REALTIME_TOTAL_STR(usToStr(us)));
    	}

    	xmlEdit->recheckAuto(); // TODO: Only if value changed
	// There's text in the cell but it's garbage, show the error icon
	} else {
		item->setIcon(xmlEdit->stopIcon);
		split.hasError = true;
	}
}

bool SingleRun::finalTotalHas(QStringList &splitNames) {
	return splits.size() > 0
	    && splits.size() == splitNames.size()
	    && splits.last().totalHas;
}
uint64_t SingleRun::finalTotal() {
	return splits.last().totalUs;
}

void SingleSplit::copyContent(SingleSplit &other, QDomDocument writeDocument) {
    splitHas = other.splitHas;
    totalHas = other.totalHas;
    splitUs = other.splitUs;
    totalUs = other.totalUs;

	if (!writeDocument.isNull())
		write(writeDocument);
}

void XmlEdit::recheckAuto() {
	if (splitNames.size() == 0) // ???
		return;

	// Auto pb
	int bestRunId = -1;
	if (bestRunAutomaticWidget->isChecked()) {
		// Choose which run is the new PB
		uint64_t bestRunTotal = 0;
		bool bestRunEqual = true;
		for(int ridx = 0; ridx < runKeys.size(); ridx++) {
	    	int id = runKeys[ridx];
	    	SingleRun &run = runs[id];
	    	if (run.finalTotalHas(splitNames)) {
	    		uint64_t finalTotal = run.finalTotal();
	    		if (bestRunId < 0 || runs[bestRunId].finalTotal() <= finalTotal) {
	    			// This is the best run we've checked so far
	    			bestRunId = id;
	    			bestRunTotal = finalTotal;
	    			bestRunEqual = true;
	    			if (run.splits.size() != bestRun.splits.size()) {
	    				bestRunEqual = false;
	    			} else {
		    			for(int sidx = 0; sidx < run.splits.size(); sidx++) {
		    				SingleSplit &split = run.splits[sidx];
		    				SingleSplit &pbSplit = bestRun.splits[sidx];

		    				if (split.totalHas != pbSplit.totalHas || (split.totalHas && split.totalUs != pbSplit.totalUs)) {
		    					bestRunEqual = false;
		    				}
		    			}
		    		}
	    		}
	    	}
	    }
	    // The new PB must be copied into bestRun?
	    if (!bestRunEqual) {
	    	SingleRun &newBestRun = runs[bestRunId];

	    	if (bestRun.splits.size() < newBestRun.splits.size()) {
	    		QMessageBox::warning(this, tr("Sorry"),
                               tr("The PB XML has fewer entries than the actual PB and currently this app can't fix that."),
                               QMessageBox::Cancel);
	    	} else {
		    	for(int sidx = 0; sidx < newBestRun.splits.size(); sidx++) {
		    		//bestRun.ensureSpaceFor(sidx);
		    		bestRun.splits[sidx].copyContent(newBestRun.splits[sidx]);
		    	}
		    }
	    }
	}
    // Flag PB
    if (bestRun.finalTotalHas(splitNames)) {
    	uint64_t bestRunTotal = bestRun.finalTotal();
	    for(int ridx = 0; ridx < runKeys.size(); ridx++) {
	    	int id = runKeys[ridx];
	    	SingleRun &run = runs[id];
	    	bool best = id == bestRunId;

	    	if (best) {
	    		run.personalBestWidget->setText(tr("(Personal Best)", "PB flag on run"));
	    	} else {
	    		bool bestEqual = run.finalTotalHas(splitNames) && run.finalTotal() == bestRunTotal;
	    		run.personalBestWidget->setText(bestEqual ? tr("(Personal Best)", "PB flag on run") : QString());
	    	}
	    }
	}


	if (bestSplitsAutomaticWidget->isChecked()) {
		for(int ridx = 0; ridx < runKeys.size(); ridx++) {
	    	int id = runKeys[ridx];
	    	SingleRun &run = runs[id];

	    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
	    	}
	    }
	}

	//item->setIcon(xmlEdit->nullIcon);
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

    clear();

    if (!domDocument.setContent(device, true, &errorStr, &errorLine,
                                &errorColumn)) {
        QMessageBox::information(window(), tr("XML Editor"),
                                 tr("Parse error at line %1, column %2:\n%3")
                                 .arg(errorLine)
                                 .arg(errorColumn)
                                 .arg(errorStr));
        return false;
    }

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
    renderRun(QString(tr("Personal Best")), bestRun, content, vContentLayout, &bestRunAutomaticWidget);
    renderRun(QString(tr("Best Splits")), bestSplits, content, vContentLayout, &bestSplitsAutomaticWidget);
    for(int ridx = 0; ridx < runKeys.size(); ridx++) {
    	int id = runKeys[ridx];
    	SingleRun &run = runs[id];

    	renderRun(QString(tr("Run %1: %2")).arg(id).arg(run.timeLabel), run, content, vContentLayout);
    }

    // Double-check tables
    correctTable(bestRun, true, false);
    correctTable(bestSplits, false, false);
    for(int ridx = 0; ridx < runKeys.size(); ridx++) {
    	int id = runKeys[ridx];
    	SingleRun &run = runs[id];
    	correctTable(run, false, false); // Runs track split time
    }
    recheckAuto(); // All this can do is flag equals

    connect(bestRunAutomaticWidget, &QCheckBox::stateChanged,
            this, &XmlEdit::checkboxChanged);
    connect(bestSplitsAutomaticWidget, &QCheckBox::stateChanged,
            this, &XmlEdit::checkboxChanged);

    return true;
}

// If truthIsTotal convert total->split otherwise do the opposite
// If changeFinalTotal then it's okay to muck with realTimeTotal
void XmlEdit::correctTable(SingleRun &run, bool truthIsTotal, bool changeFinalTotal) {
	correctingTable = true; // Create a block in time we don't trigger ::changed

	if (run.tableWidget) {
		if (truthIsTotal) { // Total is truth, fill out splits
			uint64_t lastUs = 0;
	    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
	    		SingleSplit &split = run.splits[sidx];
	    		if (split.totalHas) { // FIXME: check valid() here at some point?
	    			uint64_t splitUs = split.totalUs - lastUs;
	    			if (split.splitTimeWidget) // Update widget on screen
	    				split.splitTimeWidget->setText(usToStr(splitUs));
	    			// Update split object
	    			split.splitUs = splitUs;
	    			split.splitHas = true;
	    			// Move on
	    			lastUs = split.totalUs;
	    		} else {
	    			if (split.splitTimeWidget) // Update widget on screen
	    				split.splitTimeWidget->setText(QString());
	    			// Update split object
	    			split.splitHas = false;
	    		}
    			if (!split.xmlIsTotal) // Write changes to xml DOM
    				split.write(domDocument);
	    	}
		} else { // Splits are truth, fill out totals
			uint64_t totalUs = 0;
	    	for(int sidx = 0; sidx < run.splits.size(); sidx++) {
	    		SingleSplit &split = run.splits[sidx];
	    		if (!split.valid()) // There has been a reroute and any totals are meaningless.
	    			break;
	    		if (split.splitHas) {
	    			totalUs += split.splitUs;
	    			if (split.totalTimeWidget) // Update widget on screen
	    				split.totalTimeWidget->setText(usToStr(totalUs));
	    			// Update split object
	    			split.totalUs = totalUs;
	    			split.totalHas = true;
	    		} else {
	    			if (split.totalTimeWidget) // Update widget on screen
	    				split.totalTimeWidget->setText(QString());
	    			// Update split object
	    			split.totalHas = false;
	    		}
    			if (split.xmlIsTotal) // Write changes to xml DOM
    				split.write(domDocument);
	    	}
	    	// Handle the final "run total", which is tracked separately
	    	if (changeFinalTotal && run.splits.size() == runTableLabels.size()) {
	    		if (!run.realTimeTotal.isNull())
		    		run.realTimeTotal.setData(usToStr(totalUs));
	    		if (run.realTimeTotalWidget)
		    		run.realTimeTotalWidget->setText(REALTIME_TOTAL_STR(usToStr(totalUs)));
	    	}
	    }
	}

	correctingTable = false;
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

void XmlEdit::checkboxChanged(int) {
	recheckAuto();
}
