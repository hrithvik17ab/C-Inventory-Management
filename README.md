# Inventory Management System

A C++ command-line application that uses SQLite to keep track of products—including their name, quantity, and price—in a local `inventory.db` file. When you run the program, it initializes the database (creating a `products` table if needed) and presents an easy menu:

1. Add a product
2. View all products
3. Update a product
4. Delete a product
5. Search by name
6. Filter by quantity
7. Generate a report
8. Exit

Under the hood, each choice maps to a prepared SQL statement using the SQLite C API: parameters are bound, statements are executed, and results show up in a neatly formatted table. The report option runs simple aggregate queries to show total items and total inventory value. To use this project, compile `main.cpp` with:

```bash
g++ -std=c++11 main.cpp -lsqlite3 -o inventory
```

Then run:

```bash
./inventory
```
