#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sqlite3.h>
#include <iomanip>
#include <limits>

// Helper: Quote identifiers (table/column names) with double quotes and escape internal quotes
std::string quoteIdentifier(const std::string& id) {
    std::string q = "\"";
    for (char c : id) {
        if (c == '"') q += "\"\"";  // escape double quote by doubling
        else q += c;
    }
    q += "\"";
    return q;
}

// Utility: Split user input string by space or comma
std::vector<int> parseFieldSelection(const std::string& input, size_t maxField) {
    std::vector<int> fields;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, ' ')) {
        if (token.empty()) continue;
        std::stringstream ss2(token);
        std::string subtoken;
        while (std::getline(ss2, subtoken, ',')) {
            try {
                int num = std::stoi(subtoken);
                if (num > 0 && static_cast<size_t>(num) <= maxField)
                    fields.push_back(num);
            }
            catch (...) {
                // ignore invalid input
            }
        }
    }
    return fields;
}

std::vector<std::string> getTableNames(sqlite3* db) {
    std::vector<std::string> tables;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table';";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement to get tables." << std::endl;
        exit(1);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        tables.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return tables;
}

std::vector<std::string> getColumnNames(sqlite3* db, const std::string& table) {
    std::vector<std::string> columns;
    std::string sql = "PRAGMA table_info(" + quoteIdentifier(table) + ");";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement to get columns." << std::endl;
        exit(1);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        columns.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))); // column name at index 1
    }
    sqlite3_finalize(stmt);
    return columns;
}

void searchAndDisplay(sqlite3* db, const std::string& table,
    const std::vector<std::string>& searchFields,
    const std::vector<std::string>& searchValues,
    const std::vector<std::string>& columns) {
    if (searchFields.empty() || searchFields.size() != searchValues.size()) {
        std::cerr << " Invalid search input." << std::endl;
        return;
    }

    std::string sql = "SELECT * FROM " + quoteIdentifier(table) + " WHERE ";
    for (size_t i = 0; i < searchFields.size(); ++i) {
        sql += "TRIM(" + quoteIdentifier(searchFields[i]) + ", '\"') = ?";
        if (i < searchFields.size() - 1) sql += " AND ";
    }
    sql += ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << " Failed to prepare search query." << std::endl;
        return;
    }

    for (size_t i = 0; i < searchValues.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), searchValues[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        std::cout << "\n Match found:\n\n";
        std::cout << "_____________________________\n";
        for (size_t i = 0; i < columns.size(); ++i) {
            const unsigned char* text = sqlite3_column_text(stmt, static_cast<int>(i));
            std::string value = text ? reinterpret_cast<const char*>(text) : "";
            std::cout << std::left << std::setw(15) << columns[i] << ": " << value << '\n';
        }
        std::cout << "_____________________________\n";
    }

    if (!found) {
        std::cout << " No matching records found.\n";
    }

    sqlite3_finalize(stmt);
}

int main() {
    std::string dbPath;
    std::cout << "Enter path to your .db file: ";
    std::getline(std::cin, dbPath);

    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Get tables
    std::vector<std::string> tables = getTableNames(db);
    if (tables.empty()) {
        std::cerr << "No tables found in the database." << std::endl;
        sqlite3_close(db);
        return 1;
    }

    std::cout << "Tables found:\n";
    for (size_t i = 0; i < tables.size(); ++i) {
        std::cout << i + 1 << ": " << tables[i] << std::endl;
    }

    std::cout << "Select a table by number: ";
    size_t tableIndex;
    std::cin >> tableIndex;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (tableIndex == 0 || tableIndex > tables.size()) {
        std::cerr << "Invalid table selection." << std::endl;
        sqlite3_close(db);
        return 1;
    }
    std::string selectedTable = tables[tableIndex - 1];

    // Get columns
    std::vector<std::string> columns = getColumnNames(db, selectedTable);
    if (columns.empty()) {
        std::cerr << "No columns found in the table." << std::endl;
        sqlite3_close(db);
        return 1;
    }

    std::cout << "Fields in table '" << selectedTable << "':\n";
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << i + 1 << ": " << columns[i] << std::endl;
    }

    std::cout << "Select field/fields to search by data (separated by spaces or commas), e.g. '1' or '3 4' : ";
    std::string fieldSelection;
    std::getline(std::cin, fieldSelection);

    std::vector<int> selectedFieldIndices = parseFieldSelection(fieldSelection, columns.size());
    if (selectedFieldIndices.empty()) {
        std::cerr << "No valid fields selected." << std::endl;
        sqlite3_close(db);
        return 1;
    }

    std::vector<std::string> searchFields;
    std::vector<std::string> searchValues;

    for (int idx : selectedFieldIndices) {
        std::string val;
        std::cout << "Enter value to search for in field '" << columns[idx - 1] << "': ";
        std::getline(std::cin, val);
        searchFields.push_back(columns[idx - 1]);
        searchValues.push_back(val);
    }

    searchAndDisplay(db, selectedTable, searchFields, searchValues, columns);

    std::cout << "Press Enter to exit...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    sqlite3_close(db);
    return 0;
}
