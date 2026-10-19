#include "widgets/text/ArticleView.h"

#include "core/dictionary/DictionaryModel.h"
#include "core/palette/MaterialPalette.h"
#include "widgets/text/ArticleRuns.h"
#include "widgets/text/RubyLabel.h"
#include "widgets/text/RunLabel.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtAlgorithms>

ArticleView::ArticleView(QWidget *parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->installEventFilter(this);
    content = new QWidget(viewport());
    content->setObjectName(QStringLiteral("articleContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);
    layout->addStretch(1);
    setWidget(content);
}

void ArticleView::clearContent() {
    QLayoutItem *stretch = content->layout()->takeAt(content->layout()->count() - 1);
    qDeleteAll(content->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
    content->layout()->addItem(stretch);
    spanAnchor.clear();
    spanAnchorOff = -1;
    spanning = false;
    hasSpan = false;
}

static RunLabel *makeRuns(QWidget *parent, const QString &name) {
    auto *label = new RunLabel(parent);
    label->setObjectName(name);
    return label;
}

void ArticleView::setArticle(const ArticleDisplay &article) {
    clearContent();
    auto *layout = qobject_cast<QVBoxLayout *>(content->layout());
    // Hover tracking plus link forwarding for run labels.
    const auto track = [&](RunLabel *label) {
        label->installEventFilter(this);
        connect(label, &RunLabel::linkClicked, this, &ArticleView::linkActivated);
        connect(label, &RunLabel::kanjiDoubleClicked, this, &ArticleView::kanjiDoubleClicked);
    };
    auto *heading = new RubyLabel(content);
    heading->setObjectName(QStringLiteral("articleHeading"));
    heading->installEventFilter(this);
    connect(heading, &RubyLabel::kanjiDoubleClicked, this, &ArticleView::kanjiDoubleClicked);
    heading->setSpans(ArticleRuns::headingSpans(article));
    layout->insertWidget(layout->count() - 1, heading);
    if (const auto reading = ArticleRuns::readingLine(article); reading.has_value()) {
        auto *readingLabel = new RunLabel(content);
        readingLabel->setObjectName(QStringLiteral("articleReading"));
        track(readingLabel);
        readingLabel->setRuns({{*reading, RunLabel::Ink::Body}});
        layout->insertWidget(layout->count() - 1, readingLabel);
    }
    // Badges share one row.
    if (ArticleRuns::isCommonWord(article) || (article.jlpt >= 1 && article.jlpt <= 5)) {
        auto *badges = new QWidget(content);
        badges->setObjectName(QStringLiteral("articleBadges"));
        auto *row = new QHBoxLayout(badges);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        const MaterialPalette theme = MaterialPalette::fromPalette(QApplication::palette());
        if (ArticleRuns::isCommonWord(article)) {
            auto *common = new QLabel(tr("Common word"), badges);
            common->setObjectName(QStringLiteral("commonBadge"));
            common->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: white; "
                                                 "padding: 2px 6px; }")
                                      .arg(theme.accentGreen().name()));
            row->addWidget(common);
        }
        if (article.jlpt >= 1 && article.jlpt <= 5) {
            auto *jlpt = new QLabel(tr("JLPT N%1").arg(article.jlpt), badges);
            jlpt->setObjectName(QStringLiteral("jlptBadge"));
            jlpt->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: white; "
                                               "padding: 2px 6px; }")
                                    .arg(theme.link().name()));
            row->addWidget(jlpt);
        }
        row->addStretch(1);
        layout->insertWidget(layout->count() - 1, badges);
    }
    // One run block per sense.
    int n = 0;
    for (const SenseDisplay &sense : article.senses) {
        auto *senseLabel = new RunLabel(content);
        senseLabel->setObjectName(QStringLiteral("articleSense"));
        track(senseLabel);
        senseLabel->setRuns(ArticleRuns::senseRuns(sense, ++n));
        layout->insertWidget(layout->count() - 1, senseLabel);
    }
    // Remaining headword forms (ruby or plain plus notes).
    const QVector<ArticleRuns::FormBlock> forms = ArticleRuns::formBlocks(article);
    if (!forms.isEmpty()) {
        auto *header = new QLabel(tr("Other forms"), content);
        header->setObjectName(QStringLiteral("otherFormsHeader"));
        QFont bold = header->font();
        bold.setBold(true);
        header->setFont(bold);
        layout->insertWidget(layout->count() - 1, header);
    }
    for (const ArticleRuns::FormBlock &form : forms) {
        if (form.ruby) {
            auto *ruby = new RubyLabel(content);
            ruby->setObjectName(QStringLiteral("articleForm"));
            ruby->installEventFilter(this);
            connect(ruby, &RubyLabel::kanjiDoubleClicked, this, &ArticleView::kanjiDoubleClicked);
            ruby->setSpans(form.spans);
            layout->insertWidget(layout->count() - 1, ruby);
            if (!form.notes.isEmpty()) {
                auto *note = new RunLabel(content);
                note->setObjectName(QStringLiteral("articleFormNote"));
                track(note);
                note->setRuns({{QStringLiteral("(%1)").arg(form.notes), RunLabel::Ink::Dim, true}});
                layout->insertWidget(layout->count() - 1, note);
            }
        } else {
            auto *plain = new RunLabel(content);
            plain->setObjectName(QStringLiteral("articleForm"));
            track(plain);
            plain->setRuns({{form.plain, RunLabel::Ink::Body}});
            layout->insertWidget(layout->count() - 1, plain);
        }
    }
}

void ArticleView::refreshTheme() {
    // Application palette, not the widget's: widget palettes lag
    // behind setPalette until each widget processes its own change
    // event, while the application palette updates synchronously.
    const MaterialPalette theme = MaterialPalette::fromPalette(QApplication::palette());
    if (auto *common = content->findChild<QLabel *>(QStringLiteral("commonBadge"))) {
        common->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: white; "
                                             "padding: 2px 6px; }")
                                  .arg(theme.accentGreen().name()));
    }
    if (auto *jlpt = content->findChild<QLabel *>(QStringLiteral("jlptBadge"))) {
        jlpt->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: white; "
                                           "padding: 2px 6px; }")
                                .arg(theme.link().name()));
    }
}
