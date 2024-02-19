Excel and every other spreadsheet program that I know of is terrible at reading CSV files.  If you use CSV files regularly, you probably know what I mean.  It tries to interpret the information as data types, and it doesn't do a very good job of determining what the data is.  One example is a CSV file that I had the value "9470628211241443".  That was a reference value and I needed to be able to get that exact value, but LibreOffice interpreted it as a floating point number and displayed it as "9.47062821124144E+015".

But that's not really the fault of Excel or any other spreadsheet software.  It's just not designed for that.  Spreadsheets are for making calcuations, but CSVs are for storing data.  We use Excel to open a CSV because it kinda/sorta looks like a spreadsheet, but it's not a spreadsheet.

I've tried using SQLite instead to handle CSV files, but that actually has a completely different set of problems.  For starters, you have basically no rules on what can be a header in a CSV file, but you do have restrictions on field names in most relational database software.  Secondly, values in relational databases are almost always typed.  Values in CSV files are not typed.  CSV files and relational databases simply do not store data in the same way.

Constantly frustrated with having no good solution to viewing data in a CSV file, I finally decided to make this program to read CSV files without the problem of spreadsheets showing me misinterpreted data.

csv.c (and its header file) is taken from here-- https://github.com/semitrivial/csv\_parser/tree/master.  It's been modded by adding a delimiter option and making one static function non-static.  I am immensely grateful to its author for making this project much easier.

I'm really just making this for myself, so I'm only adding features that I need at the moment.  It only reads and does not edit, for starters.  More functions may be added as-needed in the future, but if you're reading this and you need a new feature, you'll probably need to fork the project and add it yourself.
