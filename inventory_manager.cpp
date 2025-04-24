#include <iostream> // For input/output operations
#include <string>   // For using string objects
#include <vector>   // For using vector containers
#include <limits>   // For numeric_limits (used for clearing input buffer)
#include <sqlite3.h> // For SQLite C API
#include <stdexcept> // For standard exceptions
#include <iomanip>  // For std::setprecision, std::fixed
#include <sstream> // For string streams (used in search)

// Structure to hold product data
struct Product {
    int id;
    std::string name;
    int quantity;
    double price;
};

// --- Database Interaction Functions ---

// Callback function for SELECT queries (used by sqlite3_exec for multi-row results)
// Prints rows matching the inventory table format.
static int selectCallbackPrint(void* data, int argc, char** argv, char** azColName) {
    // Print the row data formatted
    std::cout << "| " << std::left << std::setw(5) << (argv[0] ? argv[0] : "NULL"); // ID
    std::cout << "| " << std::left << std::setw(25) << (argv[1] ? argv[1] : "NULL"); // Name
    std::cout << "| " << std::right << std::setw(10) << (argv[2] ? argv[2] : "NULL"); // Quantity
    std::cout << "| $" << std::right << std::setw(9) << std::fixed << std::setprecision(2) << (argv[3] ? std::stod(argv[3]) : 0.00); // Price
    std::cout << " |" << std::endl;
    return 0; // Return 0 to continue processing rows
}

// Executes a non-SELECT SQL statement and handles errors
bool executeSQL(sqlite3* db, const std::string& sql, const std::string& successMsg = "", bool printErrors = true) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg); // Use null callback for non-SELECT

    if (rc != SQLITE_OK) {
        if (printErrors) {
            std::cerr << "SQL error: " << errMsg << std::endl;
        }
        sqlite3_free(errMsg); // Free error message memory
        return false;
    } else if (!successMsg.empty()) {
        std::cout << successMsg << std::endl;
    }
    return true;
}

// Executes a SELECT SQL statement using sqlite3_exec (suitable for simple display)
bool executeSQLSelectAndPrint(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), selectCallbackPrint, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error executing select: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
     if (sqlite3_changes(db) == 0 && rc == SQLITE_OK) {
         // Check if the query ran ok but returned no rows (sqlite3_changes isn't reliable for SELECT)
         // A more robust way is needed if we want to specifically detect "no rows found" vs error
         // For now, the callback simply won't print anything if no rows are returned.
    }
    return true;
}


// Initializes the database and creates the products table if it doesn't exist
bool initializeDatabase(sqlite3*& db, const std::string& dbName) {
    int rc = sqlite3_open(dbName.c_str(), &db); // Open the database file

    if (rc != SQLITE_OK) { // Use SQLITE_OK check
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db); // Close DB if open failed partially
        db = nullptr;
        return false;
    } else {
        std::cout << "Opened database successfully" << std::endl;
    }

    // SQL statement to create the products table
    std::string createTableSQL =
        "CREATE TABLE IF NOT EXISTS products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "quantity INTEGER NOT NULL,"
        "price REAL NOT NULL);";

    return executeSQL(db, createTableSQL, "Table 'products' checked/created successfully.");
}

// Adds a new product to the database using prepared statements
bool addProduct(sqlite3* db, const Product& product) {
    sqlite3_stmt* stmt;
    std::string sql = "INSERT INTO products (name, quantity, price) VALUES (?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (INSERT): " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind values
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, product.quantity);
    sqlite3_bind_double(stmt, 3, product.price);

    // Execute
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed (INSERT): " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    std::cout << "Product '" << product.name << "' added successfully." << std::endl;
    sqlite3_finalize(stmt);
    return true;
}

// Prints the inventory table header
void printInventoryHeader() {
     std::cout << "+-------+---------------------------+------------+------------+" << std::endl;
     std::cout << "| ID    | Name                      | Quantity   | Price      |" << std::endl;
     std::cout << "+-------+---------------------------+------------+------------+" << std::endl;
}

// Prints the inventory table footer
void printInventoryFooter() {
     std::cout << "+-------+---------------------------+------------+------------+" << std::endl;
}

// Views all products in the database
bool viewProducts(sqlite3* db) {
    std::string sql = "SELECT id, name, quantity, price FROM products;";
    std::cout << "\n--- Current Inventory ---" << std::endl;
    printInventoryHeader();
    bool success = executeSQLSelectAndPrint(db, sql);
    if (success) {
        printInventoryFooter();
    } else {
        std::cerr << "Failed to retrieve products." << std::endl;
    }
    return success;
}

// Updates an existing product in the database using prepared statements
bool updateProduct(sqlite3* db, const Product& product) {
    sqlite3_stmt* stmt;
    std::string sql = "UPDATE products SET name = ?, quantity = ?, price = ? WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (UPDATE): " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind values
    sqlite3_bind_text(stmt, 1, product.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, product.quantity);
    sqlite3_bind_double(stmt, 3, product.price);
    sqlite3_bind_int(stmt, 4, product.id);

    // Execute
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Update failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    // Check if any row was actually updated
    if (sqlite3_changes(db) == 0) {
         std::cout << "No product found with ID " << product.id << ". Update failed." << std::endl;
         sqlite3_finalize(stmt);
         return false; // Indicate that no update occurred
    }

    std::cout << "Product updated successfully." << std::endl;
    sqlite3_finalize(stmt);
    return true;
}

// Deletes a product from the database by ID using prepared statements
bool deleteProduct(sqlite3* db, int id) {
    sqlite3_stmt* stmt;
    std::string sql = "DELETE FROM products WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (DELETE): " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind the ID
    sqlite3_bind_int(stmt, 1, id);

    // Execute
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Deletion failed: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

     // Check if any row was actually deleted
    if (sqlite3_changes(db) == 0) {
         std::cout << "No product found with ID " << id << ". Deletion failed." << std::endl;
         sqlite3_finalize(stmt);
         return false; // Indicate that no deletion occurred
    }

    std::cout << "Product deleted successfully." << std::endl;
    sqlite3_finalize(stmt);
    return true;
}

// Searches for products by name (case-insensitive partial match)
bool searchProducts(sqlite3* db, const std::string& searchTerm) {
    sqlite3_stmt* stmt;
    // Use LOWER() for case-insensitive search and LIKE with % for partial match
    std::string sql = "SELECT id, name, quantity, price FROM products WHERE LOWER(name) LIKE LOWER(?);";

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (SEARCH): " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Construct the search pattern (e.g., "%term%")
    std::string searchPattern = "%" + searchTerm + "%";
    sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_STATIC);

    std::cout << "\n--- Search Results for \"" << searchTerm << "\" ---" << std::endl;
    printInventoryHeader();
    bool found = false;
    // Loop through results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        found = true;
        std::cout << "| " << std::left << std::setw(5) << sqlite3_column_int(stmt, 0); // ID
        std::cout << "| " << std::left << std::setw(25) << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)); // Name
        std::cout << "| " << std::right << std::setw(10) << sqlite3_column_int(stmt, 2); // Quantity
        std::cout << "| $" << std::right << std::setw(9) << std::fixed << std::setprecision(2) << sqlite3_column_double(stmt, 3); // Price
        std::cout << " |" << std::endl;
    }
    printInventoryFooter();

    if (!found && rc == SQLITE_DONE) {
        std::cout << "No products found matching \"" << searchTerm << "\"." << std::endl;
    } else if (rc != SQLITE_DONE) {
        // Error occurred during step
        std::cerr << "Error stepping through search results: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE; // Return true if loop completed without error
}

// Filters products by quantity less than a threshold
bool filterProductsByQuantity(sqlite3* db, int threshold) {
     sqlite3_stmt* stmt;
    std::string sql = "SELECT id, name, quantity, price FROM products WHERE quantity < ? ORDER BY quantity;";

    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (FILTER): " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind the threshold value
    sqlite3_bind_int(stmt, 1, threshold);

    std::cout << "\n--- Products with Quantity Less Than " << threshold << " ---" << std::endl;
    printInventoryHeader();
    bool found = false;
    // Loop through results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        found = true;
        std::cout << "| " << std::left << std::setw(5) << sqlite3_column_int(stmt, 0); // ID
        std::cout << "| " << std::left << std::setw(25) << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)); // Name
        std::cout << "| " << std::right << std::setw(10) << sqlite3_column_int(stmt, 2); // Quantity
        std::cout << "| $" << std::right << std::setw(9) << std::fixed << std::setprecision(2) << sqlite3_column_double(stmt, 3); // Price
        std::cout << " |" << std::endl;
    }
     printInventoryFooter();

    if (!found && rc == SQLITE_DONE) {
        std::cout << "No products found with quantity less than " << threshold << "." << std::endl;
    } else if (rc != SQLITE_DONE) {
        // Error occurred during step
        std::cerr << "Error stepping through filter results: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

// Generates a simple inventory report (total items, total value)
bool generateReport(sqlite3* db) {
    sqlite3_stmt* stmt_count;
    sqlite3_stmt* stmt_value;
    std::string sql_count = "SELECT COUNT(*) FROM products;";
    std::string sql_value = "SELECT SUM(quantity * price) FROM products;";
    int totalItems = 0;
    double totalValue = 0.0;
    bool success = true;

    // Get total item count
    int rc_count = sqlite3_prepare_v2(db, sql_count.c_str(), -1, &stmt_count, nullptr);
    if (rc_count == SQLITE_OK) {
        if (sqlite3_step(stmt_count) == SQLITE_ROW) {
            totalItems = sqlite3_column_int(stmt_count, 0);
        } else {
             std::cerr << "Failed to get item count: " << sqlite3_errmsg(db) << std::endl;
             success = false;
        }
    } else {
        std::cerr << "Failed to prepare count statement: " << sqlite3_errmsg(db) << std::endl;
        success = false;
    }
    sqlite3_finalize(stmt_count);

    // Get total inventory value
    int rc_value = sqlite3_prepare_v2(db, sql_value.c_str(), -1, &stmt_value, nullptr);
     if (rc_value == SQLITE_OK) {
        if (sqlite3_step(stmt_value) == SQLITE_ROW) {
            // Check if the result is NULL (happens if table is empty)
            if (sqlite3_column_type(stmt_value, 0) != SQLITE_NULL) {
                 totalValue = sqlite3_column_double(stmt_value, 0);
            } else {
                totalValue = 0.0; // Set to 0 if SUM returns NULL
            }
        } else {
             std::cerr << "Failed to get total value: " << sqlite3_errmsg(db) << std::endl;
             success = false;
        }
    } else {
        std::cerr << "Failed to prepare value statement: " << sqlite3_errmsg(db) << std::endl;
        success = false;
    }
    sqlite3_finalize(stmt_value);

    // Display the report
    std::cout << "\n--- Inventory Report ---" << std::endl;
    std::cout << "Total unique products: " << totalItems << std::endl;
    std::cout << "Total inventory value: $" << std::fixed << std::setprecision(2) << totalValue << std::endl;
    std::cout << "------------------------" << std::endl;

    return success;
}


// --- Helper Functions for CLI ---

// Clears the input buffer after reading input
void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Gets product details from the user (ensuring name is not empty)
Product getProductDetails(bool includeId = false) {
    Product p;
    if (includeId) {
        std::cout << "Enter Product ID to update: ";
        while (!(std::cin >> p.id) || p.id <= 0) { // Ensure ID is positive
            std::cout << "Invalid input. Please enter a positive number for ID: ";
            std::cin.clear();
            clearInputBuffer();
        }
        clearInputBuffer(); // Consume the newline after reading ID
    }

    // Loop until a non-empty name is entered
    do {
        std::cout << "Enter Product Name: ";
        std::getline(std::cin, p.name);
        if (p.name.empty()) {
            std::cout << "Product name cannot be empty. Please try again." << std::endl;
        }
    } while (p.name.empty());


    std::cout << "Enter Quantity: ";
    while (!(std::cin >> p.quantity) || p.quantity < 0) {
        std::cout << "Invalid input. Please enter a non-negative number for quantity: ";
        std::cin.clear();
        clearInputBuffer();
    }
    clearInputBuffer(); // Consume newline

    std::cout << "Enter Price: ";
    while (!(std::cin >> p.price) || p.price < 0.0) {
        std::cout << "Invalid input. Please enter a non-negative number for price: ";
        std::cin.clear();
        clearInputBuffer();
    }
    clearInputBuffer(); // Consume newline

    return p;
}

// Gets a product ID from the user for deletion
int getProductId(const std::string& action = "delete") {
    int id;
    std::cout << "Enter Product ID to " << action << ": ";
    while (!(std::cin >> id) || id <= 0) { // Ensure ID is positive
        std::cout << "Invalid input. Please enter a positive number for ID: ";
        std::cin.clear();
        clearInputBuffer();
    }
    clearInputBuffer(); // Consume newline
    return id;
}


// Displays the main menu
void displayMenu() {
    std::cout << "\n--- Inventory Management Menu ---" << std::endl;
    std::cout << "1. Add Product" << std::endl;
    std::cout << "2. View All Products" << std::endl;
    std::cout << "3. Update Product" << std::endl;
    std::cout << "4. Delete Product" << std::endl;
    std::cout << "5. Search Products by Name" << std::endl;
    std::cout << "6. Filter Products by Quantity" << std::endl;
    std::cout << "7. Generate Report" << std::endl;
    std::cout << "8. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}

// --- Main Application Logic ---

int main() {
    sqlite3* db = nullptr; // Pointer to the SQLite database connection
    std::string dbName = "inventory.db"; // Database file name

    // Initialize the database connection and table
    if (!initializeDatabase(db, dbName)) {
        return 1; // Exit if database initialization fails
    }

    int choice;
    do {
        displayMenu();
        // Input validation for menu choice
        while (!(std::cin >> choice) || choice < 1 || choice > 8) { // Updated range
             std::cout << "Invalid choice. Please enter a number between 1 and 8: ";
             std::cin.clear(); // Clear error flags
             clearInputBuffer(); // Discard invalid input
        }
        clearInputBuffer(); // Consume the newline character left by std::cin

        switch (choice) {
            case 1: { // Add Product
                std::cout << "\n--- Add New Product ---" << std::endl;
                Product newProduct = getProductDetails();
                addProduct(db, newProduct);
                break;
            }
            case 2: { // View Products
                viewProducts(db);
                break;
            }
            case 3: { // Update Product
                std::cout << "\n--- Update Product ---" << std::endl;
                viewProducts(db); // Show products first to help user choose ID
                Product updatedProduct = getProductDetails(true); // Get ID and new details
                updateProduct(db, updatedProduct);
                break;
            }
            case 4: { // Delete Product
                std::cout << "\n--- Delete Product ---" << std::endl;
                viewProducts(db); // Show products first
                int idToDelete = getProductId("delete");
                deleteProduct(db, idToDelete);
                break;
            }
            case 5: { // Search Products
                 std::cout << "\n--- Search Products by Name ---" << std::endl;
                 std::string searchTerm;
                 std::cout << "Enter search term: ";
                 std::getline(std::cin, searchTerm);
                 if (!searchTerm.empty()) {
                     searchProducts(db, searchTerm);
                 } else {
                     std::cout << "Search term cannot be empty." << std::endl;
                 }
                 break;
            }
             case 6: { // Filter Products
                 std::cout << "\n--- Filter Products by Quantity ---" << std::endl;
                 int threshold;
                 std::cout << "Enter maximum quantity threshold: ";
                  while (!(std::cin >> threshold) || threshold < 0) {
                     std::cout << "Invalid input. Please enter a non-negative number: ";
                     std::cin.clear();
                     clearInputBuffer();
                 }
                 clearInputBuffer();
                 filterProductsByQuantity(db, threshold);
                 break;
            }
             case 7: { // Generate Report
                 generateReport(db);
                 break;
            }
            case 8: { // Exit
                std::cout << "Exiting program." << std::endl;
                break;
            }
            default:
                // This case should theoretically not be reached due to input validation
                std::cout << "Invalid choice. Please try again." << std::endl;
                break;
        }
    } while (choice != 8); // Updated exit choice

    // Close the database connection before exiting
    if (db) {
        sqlite3_close(db);
        std::cout << "Database connection closed." << std::endl;
    }


    return 0; // Successful execution
}
