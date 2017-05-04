#include "qt/element/QtAutocompletionList.h"

#include <QPainter>
#include <QScrollBar>

#include "component/view/GraphViewStyle.h"
#include "qt/utility/QtDeviceScaledPixmap.h"
#include "settings/ApplicationSettings.h"
#include "settings/ColorScheme.h"
#include "utility/ResourcePaths.h"

QtAutocompletionModel::QtAutocompletionModel(QObject* parent)
	: QAbstractTableModel(parent)
{
}

QtAutocompletionModel::~QtAutocompletionModel()
{
}

void QtAutocompletionModel::setMatchList(const std::vector<SearchMatch>& matchList)
{
	m_matchList = matchList;
}

int QtAutocompletionModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return m_matchList.size();
}

int QtAutocompletionModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 6;
}

QVariant QtAutocompletionModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= int(m_matchList.size()) || role != Qt::DisplayRole)
	{
		return QVariant();
	}

	const SearchMatch& match = m_matchList[index.row()];

	switch (index.column())
	{
	case 0:
		return QString::fromStdString(match.name);
	case 1:
		return QString::fromStdString(match.text);
	case 2:
		return QString::fromStdString(match.subtext);
	case 3:
		return QString::fromStdString(match.typeName);
	case 4:
		{
			QList<QVariant> indices;
			for (const size_t idx : match.indices)
			{
				indices.push_back(quint64(idx));
			}
			return indices;
		}
	case 5:
		return match.nodeType;
	default:
		return QVariant();
	}
}

const SearchMatch* QtAutocompletionModel::getSearchMatchAt(int idx) const
{
	if (idx >= 0 && size_t(idx) < m_matchList.size())
	{
		return &m_matchList[idx];
	}
	return nullptr;
}

QString QtAutocompletionModel::longestText() const
{
	std::string str;
	for (const SearchMatch& match : m_matchList)
	{
		if (match.text.size() > str.size())
		{
			str = match.text;
		}
	}
	return QString::fromStdString(str);
}

QString QtAutocompletionModel::longestSubText() const
{
	std::string str;
	for (const SearchMatch& match : m_matchList)
	{
		if (match.subtext.size() > str.size())
		{
			str = match.subtext;
		}
	}
	return QString::fromStdString(str);
}

QString QtAutocompletionModel::longestType() const
{
	std::string str;
	for (const SearchMatch& match : m_matchList)
	{
		if (match.typeName.size() > str.size())
		{
			str = match.typeName;
		}
	}
	return QString::fromStdString(str);
}


QtAutocompletionDelegate::QtAutocompletionDelegate(QtAutocompletionModel* model, QObject* parent)
	: QStyledItemDelegate(parent)
	, m_model(model)
	, m_arrow()
{
	resetCharSizes();
}

QtAutocompletionDelegate::~QtAutocompletionDelegate()
{
}

void QtAutocompletionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    // get data
	QString name = index.data().toString();
	QString text = index.sibling(index.row(), index.column() + 1).data().toString();
	QString subtext = index.sibling(index.row(), index.column() + 2).data().toString();
	QString type = index.sibling(index.row(), index.column() + 3).data().toString();
	QList<QVariant> indices = index.sibling(index.row(), index.column() + 4).data().toList();
	Node::NodeType nodeType = static_cast<Node::NodeType>(index.sibling(index.row(), index.column() + 5).data().toInt());

	// define highlight colors
    ColorScheme* scheme = ColorScheme::getInstance().get();
	QColor fillColor("#FFFFFF");
	QColor textColor("#000000");

	if (type.size() && type != "command")
	{
		const GraphViewStyle::NodeColor& nodeColor = GraphViewStyle::getNodeColor(Node::getUnderscoredTypeString(nodeType), false);
		fillColor = QColor(nodeColor.fill.c_str());
		textColor = QColor(nodeColor.text.c_str());
	}
	else
	{
		fillColor = QColor(scheme->getSearchTypeColor(SearchMatch::getSearchTypeName(SearchMatch::SEARCH_COMMAND), "fill").c_str());
		textColor = QColor(scheme->getSearchTypeColor(SearchMatch::getSearchTypeName(SearchMatch::SEARCH_COMMAND), "text").c_str());
	}

	int top1 = 6;
	int top2 = m_charHeight1 + 3;

	// draw background
	QColor backgroundColor = option.palette.color((option.state & QStyle::State_Selected ? QPalette::Highlight : QPalette::Base));
	painter->fillRect(option.rect, backgroundColor);

    // draw highlights at indices
	QString highlightText(text.size(), ' ');
	if (indices.size())
	{
		for (int i = 0; i < indices.size(); i++)
		{
			int idx = indices[i].toInt() - (name.size() - text.size());
			if (idx < 0)
			{
				continue;
			}

			QRect rect(
				option.rect.left() + m_charWidth1 * (idx + 1) + 2,
				option.rect.top() + top1 - 1,
				m_charWidth1 + 1,
				m_charHeight1 - 1
			);
			painter->fillRect(rect, fillColor);

			highlightText[idx] = text.at(idx);
			text[idx] = ' ';
		}
	}
	else
	{
		QRect rect(option.rect.left(), option.rect.top() + top1, m_charWidth1 - 1, m_charHeight1 - 2);
		painter->fillRect(rect, fillColor);
	}

	// draw text normal
	painter->drawText(option.rect.adjusted(m_charWidth1 + 2, top1 - 3, 0, 0), Qt::AlignLeft, text);

	// draw text highlighted
	painter->save();
	QPen highlightPen = painter->pen();
	highlightPen.setColor(textColor);
	painter->setPen(highlightPen);
	painter->drawText(option.rect.adjusted(m_charWidth1 + 2, top1 - 3, 0, 0), Qt::AlignLeft, highlightText);
	painter->restore();

	// draw subtext
	if (subtext.size())
	{
		// draw arrow icon
		painter->drawPixmap(
			option.rect.left() + m_charWidth2 * 2,
			option.rect.top() + top2 + 1 + (m_charHeight2 - m_arrow.height()) / 2,
			m_arrow.pixmap()
		);

		painter->setFont(m_font2);

		QString highlightSubtext(subtext.size(), ' ');
		if (indices.size())
		{
			for (int i = 0; i < indices.size(); i++)
			{
				int idx = indices[i].toInt();
				if (idx >= subtext.size())
				{
					continue;
				}

				QRect rect(
					option.rect.left() + m_charWidth2 * (idx + 3) + 2,
					option.rect.top() + top2 + 1,
					m_charWidth2 + 1,
					m_charHeight2
				);
				painter->fillRect(rect, fillColor);

				highlightSubtext[idx] = subtext.at(idx);
				subtext[idx] = ' ';
			}
		}

		QPen typePen = painter->pen();
		typePen.setColor(scheme->getColor("search/popup/by_text").c_str());
		painter->setPen(typePen);

		// draw subtext normal
		painter->drawText(option.rect.adjusted((3 * m_charWidth2) + 2, top2, 0, 0), Qt::AlignLeft, subtext);

		// draw subtext highlighted
		painter->save();
		QPen highlightPen = painter->pen();
		highlightPen.setColor(textColor);
		painter->setPen(highlightPen);
		painter->drawText(option.rect.adjusted((3 * m_charWidth2) + 2, top2, 0, 0), Qt::AlignLeft, highlightSubtext);
		painter->restore();
	}

	// draw type
	if (type.size())
	{
		painter->setFont(m_font2);

		QPen typePen = painter->pen();
		typePen.setColor(scheme->getColor("search/popup/by_text").c_str());
		painter->setPen(typePen);

		int width = m_charWidth2 * type.size();
		int x = painter->viewport().right() - width - m_charWidth2;
		int y = option.rect.top() + top2;

		painter->fillRect(QRect(x - m_charWidth2, y, width + m_charWidth2 * 3, m_charHeight2 + 2), backgroundColor);
		painter->drawText(QRect(x, y, width, m_charHeight2), Qt::AlignRight, type);
	}

	// draw bottom line
	QRect rect(0, option.rect.bottom(), option.rect.width(), 1);
	painter->fillRect(rect, scheme->getColor("search/popup/line").c_str());

	painter->restore();
}

QSize QtAutocompletionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	const_cast<QtAutocompletionDelegate*>(this)->calculateCharSizes(option.font);

	QString text = m_model->longestText();
	QString subtext = m_model->longestSubText();
	QString type = m_model->longestType();

	return QSize(
		std::max((text.size() + 2) * m_charWidth1, (subtext.size() + type.size() + 6) * m_charWidth2),
		m_charHeight1 * 2 + 3
	);
}

void QtAutocompletionDelegate::calculateCharSizes(QFont font)
{
	if (m_charWidth1 > 0)
	{
		return;
	}

	m_font1 = font;

	QFontMetrics metrics1(font);
	m_charWidth1 = metrics1.width(
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
		) / 500.0f;
	m_charHeight1 = metrics1.height();

	font.setPixelSize(ApplicationSettings::getInstance()->getFontSize() - 3);
	m_font2 = font;

	QFontMetrics metrics2(font);
	m_charWidth2 = metrics2.width(
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
			"----------------------------------------------------------------------------------------------------"
		) / 500.0f;
	m_charHeight2 = metrics2.height();

	m_arrow = QtDeviceScaledPixmap(QString::fromStdString(ResourcePaths::getGuiPath().str() + "search_view/images/arrow.png"));
	m_arrow.scaleToWidth(m_charWidth2);
	m_arrow.colorize(ColorScheme::getInstance()->getColor("search/popup/by_text").c_str());
}

void QtAutocompletionDelegate::resetCharSizes()
{
	m_charWidth1 = m_charHeight1 = m_charWidth2 = m_charHeight2 = 0.0f;
}


QtAutocompletionList::QtAutocompletionList(QWidget* parent)
	: QCompleter(parent)
{
	m_model = std::make_shared<QtAutocompletionModel>(this);
	setModel(m_model.get());

	m_delegate = std::make_shared<QtAutocompletionDelegate>(m_model.get(), this);

	QListView* list = new QListView(parent);
	list->setItemDelegateForColumn(0, m_delegate.get());
	list->setObjectName("search_box_popup");
	list->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	list->setUniformItemSizes(true);
	setPopup(list);

	setCaseSensitivity(Qt::CaseInsensitive);
	setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	setModelSorting(QCompleter::UnsortedModel);
	setCompletionPrefix("");
	setMaxVisibleItems(8);

	m_scrollSpeedChangeListenerHorizontal.setScrollBar(list->horizontalScrollBar());
	m_scrollSpeedChangeListenerVertical.setScrollBar(list->verticalScrollBar());
}

QtAutocompletionList::~QtAutocompletionList()
{
}

void QtAutocompletionList::completeAt(const QPoint& pos, const std::vector<SearchMatch>& autocompletionList)
{
	m_model->setMatchList(autocompletionList);

	QListView* list = dynamic_cast<QListView*>(popup());
	if (!autocompletionList.size())
	{
		list->hide();
		return;
	}

	m_delegate->resetCharSizes();

	disconnect(); // must be done because of a bug where signals are no longer received by QtSmartSearchBox
	connect(this, SIGNAL(highlighted(const QModelIndex&)), this, SLOT(onHighlighted(const QModelIndex&)), Qt::DirectConnection);
	connect(this, SIGNAL(activated(const QModelIndex&)), this, SLOT(onActivated(const QModelIndex&)), Qt::DirectConnection);

	complete(QRect(pos.x(), pos.y(), std::max(dynamic_cast<QWidget*>(parent())->width(), 400), 1));

	list->setCurrentIndex(completionModel()->index(0, 0));
}

const SearchMatch* QtAutocompletionList::getSearchMatchAt(int idx) const
{
	return m_model->getSearchMatchAt(idx);
}

void QtAutocompletionList::onHighlighted(const QModelIndex& index)
{
	const SearchMatch* match = getSearchMatchAt(index.row());
	if (match)
	{
		emit matchHighlighted(*match);
	}
}

void QtAutocompletionList::onActivated(const QModelIndex& index)
{
	const SearchMatch* match = getSearchMatchAt(index.row());
	if (match)
	{
		emit matchActivated(*match);
	}
}
