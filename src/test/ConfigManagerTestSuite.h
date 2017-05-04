#include "cxxtest/TestSuite.h"

#include "utility/ConfigManager.h"
#include "utility/text/TextAccess.h"

class ConfigManagerTestSuite: public CxxTest::TestSuite
{
public:
	void test_config_manager_returns_true_when_key_is_found()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		float value;
		bool success = config->getValue("path/to/single_value", value);

		TS_ASSERT(success);
	}


	void test_config_manager_returns_false_when_key_is_not_found()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		float value;
		bool success = config->getValue("path/to/nowhere", value);

		TS_ASSERT(!success);
	}


	void test_config_manager_returns_correct_string_for_key()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		std::string value;
		config->getValue("path/to/single_value", value);

		TS_ASSERT_EQUALS("42", value);
	}


	void test_config_manager_returns_correct_float_for_key()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		float value;
		config->getValue("path/to/single_value", value);

		TS_ASSERT_DELTA(42.0f, value, 0.0001f);
	}

	void test_config_manager_returns_correct_bool_for_key_if_value_is_true()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		float value;
		bool success(config->getValue("path/to/bool_that_is_true", value));

		TS_ASSERT(success);
		TS_ASSERT(value);
	}

	void test_config_manager_returns_correct_bool_for_key_if_value_is_false()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		float value;
		bool success(config->getValue("path/to/bool_that_is_false", value));

		TS_ASSERT(success);
		TS_ASSERT(!value);
	}

	void test_config_manager_adds_new_key_when_empty()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createEmpty();

		config->setValue("path/to/true_bool", true);

		bool value = false;
		bool success(config->getValue("path/to/true_bool", value));

		TS_ASSERT(success);
		TS_ASSERT(value);
	}

	void test_config_manager_adds_new_key_when_not_empty()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		config->setValue("path/to/true_bool", true);

		bool value = false;
		bool success(config->getValue("path/to/true_bool", value));

		TS_ASSERT(success);
		TS_ASSERT(value);
	}

	void test_config_manager_returns_correct_list_for_key()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());

		std::vector<int> values;

		bool success(config->getValues("paths/path", values));

		TS_ASSERT(success);
		TS_ASSERT_EQUALS(values.size(), 3);
		TS_ASSERT_EQUALS(values[0], 2);
		TS_ASSERT_EQUALS(values[1], 5);
		TS_ASSERT_EQUALS(values[2], 8);
	}

	void test_config_manager_save_and_load_configuration_and_compare()
	{
		std::shared_ptr<ConfigManager> config = ConfigManager::createAndLoad(getConfigTextAccess());
		config->save("data/temp.xml");
		std::shared_ptr<ConfigManager> config2 = ConfigManager::createAndLoad(TextAccess::createFromFile(FilePath("data/temp.xml")));
		TS_ASSERT_EQUALS(config->toString(), config2->toString());
	}

private:
	std::shared_ptr<TextAccess> getConfigTextAccess()
	{
		std::string text =
			"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
			"<config>\n"
				"<path>\n"
					"<to>\n"
						"<bool_that_is_false>0</bool_that_is_false>\n"
						"<bool_that_is_true>1</bool_that_is_true>\n"
						"<single_value>42</single_value>\n"
					"</to>\n"
				"</path>\n"
				"<paths>\n"
					"<nopath>4</nopath>\n"
					"<path>2</path>\n"
					"<path>5</path>\n"
					"<path>8</path>\n"
				"</paths>\n"
			"</config>\n";
		return TextAccess::createFromString(text);
	}

};
