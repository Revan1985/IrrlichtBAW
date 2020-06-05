#include "CommandLineHandler.hpp"

using namespace irr;
using namespace asset;
using namespace core;

CommandLineHandler::CommandLineHandler(core::vector<std::string> argv, IAssetManager* am) : status(false)
{
	core::vector<std::array<std::string, PROPER_CMD_ARGUMENTS_AMOUNT>> argvMappedList;

	auto pushArgvList = [&](auto argvStream, auto variableCount)
	{
		auto& back = argvMappedList.emplace_back();
		for (auto i = 0; i < variableCount; ++i)
			back[i] = argvStream[i];
	};

	auto getBatchFilesArgvStream = [&](std::string& fileStream) -> std::vector<std::string>
	{
		std::regex regex{"^"};
		std::sregex_token_iterator it{ fileStream.begin(), fileStream.end(), regex, -1 };
		return { it, {} };
	};

	auto getSerializedValues = [&](const auto& variablesStream, auto supposedArgumentsAmout, bool onlyEntireArgvArgument = false)
	{
		core::vector<std::string> variablesHandle;
		variablesHandle.reserve(supposedArgumentsAmout);

		std::string tmpStream;
		for (auto x = 0ul; x < variablesStream.size(); ++x)
		{
			const auto character = variablesStream.at(x);

			if (onlyEntireArgvArgument ? (character == ' ') : (character == ','))
			{
				variablesHandle.push_back(tmpStream);
				tmpStream.clear();
			}
			else if (x == variablesStream.size() - 1)
			{
				tmpStream.push_back(character);
				variablesHandle.push_back(tmpStream);
				tmpStream.clear();
			}
			else if (character == '\r' || character == '\n')
			{
				variablesHandle.push_back(tmpStream);
				break;
			}
			else
				tmpStream.push_back(character);
		}

		return variablesHandle;
	};

	if (argv.size() == PROPER_CMD_ARGUMENTS_AMOUNT)
		mode = CLM_CMD_LIST;
	else if (argv.size() == PROPER_BATCH_FILE_ARGUMENTS_AMOUNT)
		mode = CLM_BATCH_INPUT;
	else if (argv.size() > 1 && argv.size() < MANDATORY_CMD_ARGUMENTS_AMOUNT - 1)
	{
		os::Printer::log("Single argument assumptions aren't allowed - too few arguments!", ELL_ERROR);
		os::Printer::log(requiredArgumentsMessage.data(), ELL_INFORMATION);
		return;
	}
	else if (argv.size() > PROPER_CMD_ARGUMENTS_AMOUNT)
	{
		os::Printer::log("Too many arguments!", ELL_ERROR);
		os::Printer::log(requiredArgumentsMessage.data(), ELL_INFORMATION);
		return;
	}
	else
	{
		mode = CLM_UNKNOWN;
		os::Printer::log(requiredArgumentsMessage.data(), ELL_INFORMATION);
		return;
	}

	if (std::string(argv[1]) == "-batch")
	{
		auto file = am->getFileSystem()->createAndOpenFile(argv[2].c_str());
		std::string fileStream;
		fileStream.resize(file->getSize(), ' ');
		file->read(fileStream.data(), file->getSize());
		fileStream += "\r\n";

		bool error = false;
		const auto batchInputStream = getBatchFilesArgvStream(fileStream);

		for (auto i = 0ul; i < batchInputStream.size(); ++i)
		{
			const auto argvStream = *(batchInputStream.begin() + i);
			// protection against empty lines
			if (!std::regex_search(argvStream,std::regex{"[^[:s:]]"}))
				continue;

			const auto arguments = getSerializedValues(argvStream, PROPER_CMD_ARGUMENTS_AMOUNT, true);

			if (arguments.size() < MANDATORY_CMD_ARGUMENTS_AMOUNT || arguments.size() > PROPER_CMD_ARGUMENTS_AMOUNT)
			{
				error = true;
				break;
			}

			pushArgvList(arguments, arguments.size());
		}

		if (error)
		{
			os::Printer::log(requiredArgumentsMessage.data(), ELL_ERROR);
			return;
		}
	}
	else if (argv.size() == PROPER_CMD_ARGUMENTS_AMOUNT)
		pushArgvList(argv, argv.size());
	else
	{
		os::Printer::log("Invalid syntax!", ELL_ERROR);
		os::Printer::log(requiredArgumentsMessage.data(), ELL_INFORMATION);
	}

	rawVariables.resize(argvMappedList.size());
	for (auto inputBatchStride = 0ul; inputBatchStride < argvMappedList.size(); ++inputBatchStride)
	{
		const auto cmdArgumentsPerFile = *(argvMappedList.begin() + inputBatchStride);
		auto& rawVariablesHandle = rawVariables[inputBatchStride]; // unorederd map of variables
		initializeMatchingMap(rawVariablesHandle);

		for (auto argumentIterator = 0; argumentIterator < cmdArgumentsPerFile.size(); ++argumentIterator)
		{
			std::string rawFetchedCmdArgument = cmdArgumentsPerFile[argumentIterator];

			const auto offset = rawFetchedCmdArgument.find_first_of("-") + 1;
			const auto endOfFetchedVariableName = rawFetchedCmdArgument.find_first_of("=");
			const auto count = endOfFetchedVariableName - offset;
			const auto cmdFetchedVariable = rawFetchedCmdArgument.substr(offset, count);
			{
				std::string variable = cmdFetchedVariable;
				auto matchedVariableID = getMatchedVariableMapID(variable);

				if (matchedVariableID == DTEA_COUNT)
					continue;

				const auto beginningOfVariables = rawFetchedCmdArgument.find_last_of("=") + 1;
				auto variablesStream = rawFetchedCmdArgument.substr(beginningOfVariables);

				auto assignToMap = [&](DENOISER_TONEMAPPER_EXAMPLE_ARGUMENTS argument, size_t reservedSpace = 1)
				{
					auto variablesHandle = getSerializedValues(variablesStream, reservedSpace);
					auto& reference = rawVariablesHandle[argument];
					reference.emplace(variablesHandle);
				};

				if (variable == TONEMAPPER)
				{
					auto begin = rawFetchedCmdArgument.find_first_of('=')+1u;
					auto end = rawFetchedCmdArgument.find_first_of('=',begin);
					auto foundOperator = rawFetchedCmdArgument.substr(begin,end-begin);
					static const core::set<std::string> acceptedOperators = { std::string(REINHARD),std::string(ACES),std::string(NONE) };

					if (acceptedOperators.find(foundOperator)!=acceptedOperators.end())
						variable = foundOperator;
					else
					{
						os::Printer::log("ERROR (" + std::to_string(__LINE__) + " line): Invalid tonemapper specified! Id of input stride: " + std::to_string(inputBatchStride), ELL_ERROR);
						return;
					}
				}

				bool status = true;
				if (variable == COLOR_FILE)
					assignToMap(DTEA_COLOR_FILE);
				else if (variable == CAMERA_TRANSFORM)
					assignToMap(DTEA_CAMERA_TRANSFORM, 9);
				else if (variable == MEDIAN_FILTER_RADIUS)
					assignToMap(DTEA_MEDIAN_FILTER_RADIUS);
				else if (variable == DENOISER_EXPOSURE_BIAS)
					assignToMap(DTEA_DENOISER_EXPOSURE_BIAS);
				else if (variable == DENOISER_BLEND_FACTOR)
					assignToMap(DTEA_DENOISER_BLEND_FACTOR);
				else if (variable == BLOOM_FOV)
					assignToMap(DTEA_BLOOM_FOV);
				else if (variable == REINHARD)
					assignToMap(DTEA_REINHARD, 2);
				else if (variable == ACES)
					assignToMap(DTEA_ACES, 2);
				else if (variable == OUTPUT)
					assignToMap(DTEA_OUTPUT);
				else if (variable == ALBEDO_FILE)
					assignToMap(DTEA_ALBEDO_FILE);
				else if (variable == NORMAL_FILE)
					assignToMap(DTEA_NORMAL_FILE);
				else if (variable == COLOR_CHANNEL_NAME)
					assignToMap(DTEA_COLOR_CHANNEL_NAME);
				else if (variable == ALBEDO_CHANNEL_NAME)
					assignToMap(DTEA_ALBEDO_CHANNEL_NAME);
				else if (variable == NORMAL_CHANNEL_NAME)
					assignToMap(DTEA_NORMAL_CHANNEL_NAME);
				else if (variable == BLOOM_PSF_FILE)
					assignToMap(DTEA_BLOOM_PSF_FILE);
				else
				{
					os::Printer::log("ERROR (" + std::to_string(__LINE__) + " line): Unexcepted argument! Id of input stride: " + std::to_string(inputBatchStride), ELL_ERROR);
					assert(status = false);
				}
			}
		}

		if (!validateMandatoryParameters(rawVariablesHandle, inputBatchStride))
			return;
	}

	performFInalAssignmentStepForUsefulVariables();
	status = true;
}

bool CommandLineHandler::validateMandatoryParameters(const variablesType& rawVariablesPerFile, const size_t idOfInput)
{
	static const irr::core::vector<DENOISER_TONEMAPPER_EXAMPLE_ARGUMENTS> mandatoryArgumentsOrdinary = { DTEA_COLOR_FILE, DTEA_CAMERA_TRANSFORM, DTEA_MEDIAN_FILTER_RADIUS, DTEA_DENOISER_EXPOSURE_BIAS, DTEA_DENOISER_BLEND_FACTOR, DTEA_BLOOM_FOV, DTEA_OUTPUT };

	auto log = [&](bool status, const std::string message)
	{
		os::Printer::log("ERROR (" + std::to_string(__LINE__) + " line): " + message + " Id of input stride: " + std::to_string(idOfInput), ELL_ERROR);
		assert(status);
	};

	auto validateOrdinary = [&](const DENOISER_TONEMAPPER_EXAMPLE_ARGUMENTS argument)
	{
		return rawVariablesPerFile.at(argument).has_value();
	};

	auto validateTonemapper = [&]()
	{
		bool reinhardFound = rawVariablesPerFile.at(DTEA_REINHARD).has_value();
		bool acesFound = rawVariablesPerFile.at(DTEA_ACES).has_value();

		if (reinhardFound && acesFound)
			log(status = false, "Only one tonemapper can be specified at once!");

		if (reinhardFound)
		{
			if (rawVariablesPerFile.at(DTEA_REINHARD).value().size() < 2)
				log(status = false, "The Reinhard tonemapper doesn't have 2 arguments!");
		}
		else if (acesFound)
		{
			if (rawVariablesPerFile.at(DTEA_ACES).value().size() < 2)
				log(status = false, "The Aces tonemapper doesn't have 2 arguments!");
		}
		else
			log(status = false, "None tonemapper has been specified");

		return true;
	};

	for (const auto& mandatory : mandatoryArgumentsOrdinary)
	{
		bool status = validateOrdinary(mandatory);
		if (!status)
		{
			log(status, "Mandatory argument missing or it doesn't contain any value!");
			return false;
		}
	}

	return validateTonemapper();
}

std::optional<std::string> CommandLineHandler::getNormalFileName(uint64_t id)
{
	bool ableToReturn = rawVariables[id][DTEA_NORMAL_FILE].has_value() && !rawVariables[id][DTEA_NORMAL_FILE].value().empty();
	if (ableToReturn)
	{
		ableToReturn = rawVariables[id][DTEA_ALBEDO_FILE].has_value() && !rawVariables[id][DTEA_ALBEDO_FILE].value().empty();
		if (!ableToReturn)
		{
			os::Printer::log("WARNING (" + std::to_string(__LINE__) + " line): Couldn't accept normal file due to lack of albedo file! Id of input stride: " + std::to_string(id), ELL_WARNING);
			return {};
		}
	}
	else
		return {};

	return rawVariables[id][DTEA_NORMAL_FILE].value()[0];
}