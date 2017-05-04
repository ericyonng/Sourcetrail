#include "IDECommunicationController.h"

#include "utility/file/FileSystem.h"
#include "utility/messaging/type/MessageActivateSourceLocations.h"
#include "utility/messaging/type/MessageActivateWindow.h"
#include "utility/messaging/type/MessageDispatchWhenLicenseValid.h"
#include "utility/messaging/type/MessageProjectNew.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/messaging/type/MessageActivateFile.h"
#include "utility/messaging/type/MessagePingReceived.h"

#include "data/access/StorageAccess.h"
#include "data/location/SourceLocationFile.h"
#include "settings/ApplicationSettings.h"

IDECommunicationController::IDECommunicationController(StorageAccess* storageAccess)
	: m_storageAccess(storageAccess)
	, m_enabled(true)
{
}

IDECommunicationController::~IDECommunicationController()
{
}

void IDECommunicationController::clear()
{
}

void IDECommunicationController::handleIncomingMessage(const std::string& message)
{
	if (m_enabled == false)
	{
		return;
	}

	NetworkProtocolHelper::MESSAGE_TYPE type = NetworkProtocolHelper::getMessageType(message);

	if (type == NetworkProtocolHelper::MESSAGE_TYPE::UNKNOWN)
	{
		LOG_ERROR_STREAM(<< "Invalid message type");
	}
	else if (type == NetworkProtocolHelper::MESSAGE_TYPE::SET_ACTIVE_TOKEN)
	{
		handleSetActiveTokenMessage(NetworkProtocolHelper::parseSetActiveTokenMessage(message));
	}
	else if (type == NetworkProtocolHelper::MESSAGE_TYPE::CREATE_CDB_MESSAGE)
	{
		handleCreateCDBProjectMessage(NetworkProtocolHelper::parseCreateCDBProjectMessage(message));
	}
	else if (type == NetworkProtocolHelper::MESSAGE_TYPE::PING)
	{
		handlePing(NetworkProtocolHelper::parsePingMessage(message));
	}
	else
	{
		handleCreateProjectMessage(NetworkProtocolHelper::parseCreateProjectMessage(message));
	}
}

bool IDECommunicationController::getEnabled() const
{
	return m_enabled;
}

void IDECommunicationController::setEnabled(const bool enabled)
{
	m_enabled = enabled;
}

void IDECommunicationController::sendInitialPing()
{
	sendUpdatePing();
}

void IDECommunicationController::handleSetActiveTokenMessage(
	const NetworkProtocolHelper::SetActiveTokenMessage& message
)
{
	if (message.valid)
	{
		const unsigned int cursorColumn = message.column;

		if (FileSystem::getFileInfoForPath(FilePath(message.fileLocation)).lastWriteTime
			== m_storageAccess->getFileInfoForFilePath(FilePath(message.fileLocation)).lastWriteTime)
		{
			// file was not modified
			std::shared_ptr<SourceLocationFile> sourceLocationFile = m_storageAccess->getSourceLocationsForLinesInFile(
				FilePath(message.fileLocation), message.row, message.row
			);

			std::vector<Id> selectedLocationIds;
			sourceLocationFile->forEachStartSourceLocation(
				[&selectedLocationIds, &cursorColumn](SourceLocation* startLocation)
				{
					const SourceLocation* endLocation = startLocation->getEndLocation();

					if (!startLocation->isScopeLocation()
						&& startLocation->getLineNumber() == endLocation->getLineNumber()
						&& startLocation->getColumnNumber() <= cursorColumn
						&& endLocation->getColumnNumber() + 1 >= cursorColumn)
					{
						selectedLocationIds.push_back(startLocation->getLocationId());
					}
				}
			);

			if (selectedLocationIds.size() > 0)
			{
				MessageStatus("Activating source location from plug-in succeeded: " + message.fileLocation + ", row: " +
					std::to_string(message.row) + ", col: " + std::to_string(message.column)).dispatch();

				MessageActivateSourceLocations(selectedLocationIds).dispatch();
				MessageActivateWindow().dispatch();
				return;
			}
		}

		Id fileId = m_storageAccess->getNodeIdForFileNode(FilePath(message.fileLocation));
		if (fileId > 0)
		{
			MessageActivateFile(FilePath(message.fileLocation), message.row).dispatchImmediately();
			MessageActivateWindow().dispatch();
		}
		else
		{
			MessageStatus(
				"Activating source location from plug-in failed. File " + message.fileLocation
				+ " was not found in the project.",
				true
			).dispatch();
		}
	}
}

void IDECommunicationController::handleCreateProjectMessage(const NetworkProtocolHelper::CreateProjectMessage& message)
{
	if (message.valid)
	{
		if (message.ideId == "vs")
		{
			std::shared_ptr<MessageProjectNew> msg = std::make_shared<MessageProjectNew>();
			msg->setSolutionPath(message.solutionFileLocation);
			msg->ideId = message.ideId;
			MessageDispatchWhenLicenseValid(msg).dispatch();
		}
		else
		{
			LOG_ERROR_STREAM(<< "Unable to parse provided solution, unknown format");
		}
	}
}

void IDECommunicationController::handleCreateCDBProjectMessage(const NetworkProtocolHelper::CreateCDBProjectMessage& message)
{
	if (message.valid)
	{
		std::shared_ptr<MessageProjectNew> msg = std::make_shared<MessageProjectNew>();
		msg->setSolutionPath(message.cdbFileLocation);
		msg->setHeaderPaths(message.headerPaths);
		msg->ideId = message.ideId;

		MessageDispatchWhenLicenseValid(msg).dispatch();
	}
	else
	{
		LOG_ERROR_STREAM(<< "Unable to parse provided CDB, invalid data received");
	}
}

void IDECommunicationController::handlePing(const NetworkProtocolHelper::PingMessage& message)
{
	if (message.valid)
	{
		MessagePingReceived msg;
		msg.ideName = message.ideId;

		if (msg.ideName.empty())
		{
			msg.ideName = "unknown IDE";
		}

		LOG_INFO(msg.ideName + " instance detected via plugin port");
		msg.dispatch();
	}
	else
	{
		LOG_ERROR("Can't handle ping, message is invalid");
	}
}

void IDECommunicationController::handleMessage(MessageWindowFocus* message)
{
	sendUpdatePing();
}

void IDECommunicationController::handleMessage(MessageIDECreateCDB* message)
{
	std::string networkMessage = NetworkProtocolHelper::buildCreateCDBMessage();

	MessageStatus("Requesting IDE to create Compilation Database via plug-in.").dispatch();

	sendMessage(networkMessage);
}

void IDECommunicationController::handleMessage(MessageMoveIDECursor* message)
{
	std::string networkMessage = NetworkProtocolHelper::buildSetIDECursorMessage(
		message->FilePosition, message->Row, message->Column
	);

	MessageStatus(
		"Jump to source location via plug-in: " + message->FilePosition + ", row: " +
		std::to_string(message->Row) + ", col: " + std::to_string(message->Column)
	).dispatch();

	sendMessage(networkMessage);
}

void IDECommunicationController::handleMessage(MessagePluginPortChange* message)
{
	stopListening();
	startListening();
}

void IDECommunicationController::sendUpdatePing()
{
	// first reset connection status
	MessagePingReceived msg;
	msg.ideId = "";
	msg.ideName = "";
	msg.dispatch();

	// send ping to update connection status
	sendMessage(NetworkProtocolHelper::buildPingMessage());
}
