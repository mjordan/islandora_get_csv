# Islandora Metadata CSV

## Overview

Islandora 7.x utility module to generate a CSV file of the metadata used in a collection. To generate a CSV file, a user does the following:

1. Go to a collection's "Manage" tab
1. Click on the "Collection" subtab
1. Click on the "Generate CSV file of metadata used in this collection" vertical tab
   ![The menu](docs/images/collection_menu.png)
1. Select how you want to define members of a collection. "Is Member Of" will include only direct children of the current collection; "Ancestors" will include all objects that have the current collection as an ancestor. Note that using "Ancestors" requires that your Solr index contains the "ancestors_ms" field, e.g. for searching within collections.
1. Click on the "Generate metadata CSV" button.
   ![The button](docs/images/collection_button.png)
1. Download the file.
   ![The link](docs/images/collection_download.png)

The list only contains rows for objects that are direct children of the current collection, and the columns in the CSV file are filtered to those Solr fields that have at least one value in the collection:

![Sample CSV](docs/images/csv.png)

Values of multivalued fields are subdelimited by commas.

## Drush interface

You can also generate a CSV using Drush:

`drush islandora-get-csv-get-csv --collection_pid=islandora:sp_basic_image_collection --output_file=/tmp/output.csv`

## Installation

1. `git clone https://github.com/mjordan/islandora_get_csv.git`
1. `drush en -y islandora_get_csv` or enable it at Admin > Modules.

## Configuration

Admin settings are at `admin/islandora/tools/islandora_get_csv`.

Users need the 'Manage collection policies' permission to see the "Generate CSV file of metadata used in this collection" link and generate a CSV file.

## Dependencies

* [Islandora](https://github.com/Islandora/islandora)
* [Islandora Solr](https://github.com/Islandora/islandora_solr_search)
* [Islandora Collection Solution Pack](https://github.com/Islandora/islandora_solution_pack_collection)

## License

* [GPLv3](http://www.gnu.org/licenses/gpl-3.0.txt)
