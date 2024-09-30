# Search Engine

This is a simple search engine project consisting of two main components: the Spider (web crawler) and the Search Server.

## Project Structure

- `spider/`: Contains the source code for the web crawler.
- `search_engine/`: Contains the source code for the search server.
- `database/`: Contains the source code for database interactions.
- `config/`: Contains the configuration files.

## Description

The project is designed to crawl web pages starting from a given URL, index the content, and allow users to search for words within the crawled pages. The search results are ranked based on the frequency of the searched words on each page.

## Example of Program Operation

### Spider

The **Spider** starts crawling from a specified URL, collects links up to a certain recursion depth, and stores the pages' content and word frequencies in a database.

#### Example:

- **Starting URL:** `https://www.w3schools.com/`
- **Recursion Depth:** `1`
- **Collected URLs:** `271 URLs`


### Search Engine

The **Search Server** allows users to search for words, and it returns a list of URLs ranked by the frequency of the searched words on each page.

#### Example Search

Suppose we search for the words `"where"` and `"while"`.

##### Results:

1. **URL:** [https://pathfinder.w3schools.com](https://pathfinder.w3schools.com)
   - Frequency of the word `"where"`: `32` (from results of searching a single word)
   - Frequency of the word `"while"`: `0` (not mentioned)
   - **Total Frequency:** `32 + 0 = 32`
   - **Position in Results:** `1`

2. **URL:** [https://www.w3schools.com/training/aws/home/](https://www.w3schools.com/training/aws/home/)
   - Frequency of the word `"where"`: `8`
   - Frequency of the word `"while"`: `12`
   - **Total Frequency:** `8 + 12 = 20`
   - **Position in Results:** `2`

3. **URL:** [https://www.w3schools.com/sql/sql_ref_keywords.asp](https://www.w3schools.com/sql/sql_ref_keywords.asp)
   - Frequency of the word `"where"`: `13`
   - Frequency of the word `"while"`: `1`
   - **Total Frequency:** `13 + 1 = 14`
   - **Position in Results:** `3`

4. **URL:** [https://www.w3schools.com/java/java_ref_keywords.asp](https://www.w3schools.com/java/java_ref_keywords.asp)
   - Frequency of the word `"where"`: `5`
   - Frequency of the word `"while"`: `9`
   - **Total Frequency:** `5 + 9 = 14`
   - **Position in Results:** `4`

5. **URL:** [https://www.w3schools.com/php/php_examples.asp](https://www.w3schools.com/php/php_examples.asp)
   - Frequency of the word `"where"`: `6`
   - Frequency of the word `"while"`: `8`
   - **Total Frequency:** `6 + 8 = 14`
   - **Position in Results:** `5`

#### Explanation

- **Total Frequency Calculation:** For each URL, the frequencies of the searched words are summed to get the total frequency.
- **Ranking:** The results are sorted in descending order based on the total frequency. Higher total frequency means a higher position in the results.
- **Position in Results:** Indicates the rank of the URL in the search results.