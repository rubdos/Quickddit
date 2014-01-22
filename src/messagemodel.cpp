/*
    Quickddit - Reddit client for mobile phones
    Copyright (C) 2014  Dickson Leong

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see [http://www.gnu.org/licenses/].
*/

#include "messagemodel.h"

#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkReply>

#include "utils.h"
#include "parser.h"

MessageModel::MessageModel(QObject *parent) :
    AbstractListModelManager(parent), m_section(AllSection), m_reply(0)
{
#if (QT_VERSION <= QT_VERSION_CHECK(5,0,0))
    setRoleNames(customRoleNames());
#endif
}

void MessageModel::classBegin()
{
}

void MessageModel::componentComplete()
{
    refresh(false);
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_messageList.count();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    const MessageObject message = m_messageList.at(index.row());

    switch (role) {
    case FullnameRole: return message.fullname();
    case AuthorRole: return message.author();
    case BodyRole: return message.body();
    case CreatedRole: return Utils::getTimeDiff(message.created());
    case SubjectRole: return message.subject();
    case LinkTitleRole: return message.linkTitle();
    case SubredditRole: return message.subreddit();
    case ContextRole: return message.context();
    case IsCommentRole: return message.isComment();
    case IsUnreadRole: return message.isUnread();
    default:
        qCritical("MessageModel::data(): Invalid role");
        return QVariant();
    }
}

MessageModel::Section MessageModel::section() const
{
    return m_section;
}

void MessageModel::setSection(MessageModel::Section section)
{
    if (m_section != section) {
        m_section = section;
        emit sectionChanged();
    }
}

void MessageModel::refresh(bool refreshOlder)
{
    if (m_reply != 0) {
        qWarning("MessageModel::refresh(): Aborting acitve network request (Try to aviod!)");
        m_reply->disconnect();
        m_reply->deleteLater();
        m_reply = 0;
    }

    QString relativeUrl = "/message";
    switch (m_section) {
    case AllSection: relativeUrl += "/inbox"; break;
    case UnreadSection: relativeUrl += "/unread"; break;
    case MessageSection: relativeUrl += "/messages"; break;
    case CommentRepliesSection: relativeUrl += "/comments"; break;
    case PostRepliesSection: relativeUrl += "/selfreply"; break;
    case SentSection: relativeUrl += "/sent"; break;
    default: qCritical("MessageModel::refresh(): Invalid section"); break;
    }

    QHash<QString, QString> parameters;
    parameters["mark"] = "false";
    parameters["limit"] = "50";

    if (!m_messageList.isEmpty()) {
        if (refreshOlder) {
            parameters["after"] = m_messageList.last().fullname();
            parameters["count"] = m_messageList.count();
        } else {
            beginRemoveRows(QModelIndex(), 0, m_messageList.count() - 1);
            m_messageList.clear();
            endRemoveRows();
        }
    }

    setBusy(true);

    connect(manager(), SIGNAL(networkReplyReceived(QNetworkReply*)), SLOT(onNetweorkReplyReceived(QNetworkReply*)));
    manager()->createRedditRequest(QuickdditManager::GET, relativeUrl, parameters);
}

void MessageModel::changeIsUnread(const QString &fullname, bool isUnread)
{
    QList<MessageObject>::iterator i;
    for (i = m_messageList.begin(); i != m_messageList.end(); ++i) {
        if (i->fullname() == fullname) {
            i->setUnread(isUnread);
            int changedIndex = i - m_messageList.begin();
            emit dataChanged(index(changedIndex), index(changedIndex));
            break;
        }
    }
}

QHash<int, QByteArray> MessageModel::customRoleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FullnameRole] = "fullname";
    roles[AuthorRole] = "author";
    roles[BodyRole] = "body";
    roles[CreatedRole] = "created";
    roles[SubjectRole] = "subject";
    roles[LinkTitleRole] = "linkTitle";
    roles[SubredditRole] = "subreddit";
    roles[ContextRole] = "context";
    roles[IsCommentRole] = "isComment";
    roles[IsUnreadRole] = "isUnread";
    return roles;
}

void MessageModel::onNetweorkReplyReceived(QNetworkReply *reply)
{
    disconnect(manager(), SIGNAL(networkReplyReceived(QNetworkReply*)), this,
               SLOT(onNetweorkReplyReceived(QNetworkReply*)));
    if (reply != 0) {
        m_reply = reply;
        m_reply->setParent(this);
        connect(m_reply, SIGNAL(finished()), SLOT(onFinished()));
    } else {
        setBusy(false);
    }
}

void MessageModel::onFinished()
{
    if (m_reply->error() == QNetworkReply::NoError) {
        const QList<MessageObject> messageList = Parser::parseMessageList(m_reply->readAll());
        if (!messageList.isEmpty()) {
            beginInsertRows(QModelIndex(), m_messageList.count(), m_messageList.count() + messageList.count() - 1);
            m_messageList.append(messageList);
            endInsertRows();
        }
    } else {
        emit error(m_reply->errorString());
    }

    setBusy(false);
    m_reply->deleteLater();
    m_reply = 0;
}
