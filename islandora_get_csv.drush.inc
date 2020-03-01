<?php

/**
 * @file
 * Drush command for generating a CSV file containing all metadata used in a collection.
 */

/**
 * Implements hook_drush_command().
 */
function islandora_get_csv_drush_command() {
  return array(
    'islandora-get-csv-get-csv' => array(
      'aliases' => array('igcget'),
      'description' => dt('Generates a CSV file of all metadata used in an Islandora collection.'),
      'drupal dependencies' => array(
        'islandora',
        'islandora_solr',
        'islandora_basic_collection',
      ),
      'options' => array(
        'collection_pid' => array(
          'description' => dt('The PID of the collection you want to get metadata for.'),
          'required' => TRUE,
        ),
        'output_file' => array(
          'description' => dt('Absolute or relative path to the output file.'),
          'required' => TRUE,
        ),
      ),
      'examples' => array(
        'Get metadata for all items in the Islandora Basic Image collection' => 'drush islandora-get-csv-get-csv --collection_pid=islandora:sp_basic_image_collection --output_file=/tmp/output.csv',
      ),
    ),
  );
}

function drush_islandora_get_csv_get_csv() {
  module_load_include('inc', 'islandora_get_csv', 'includes/utilities');
  $pid = trim(drush_get_option('collection_pid'));

  // Check that the specified collection object exists and is
  // accessible; if it's not, exit with an error. Also confirm
  // that the object has the expected content model.
  if (drush_get_option('collection_pid')) {
    if (!islandora_object_load(drush_get_option('collection_pid'))) {
      drush_set_error('COLLECTION_NOT_FOUND',
        dt('The specified collection object (!collection) is not found or is not accessible.',
        array('!collection' => drush_get_option('collection_pid'))));
        drush_user_abort(dt('Exiting, no datastreams fetched.'));
        exit;

    }
    else {
      $collection_object = islandora_object_load(drush_get_option('collection_pid'));
      $collection_cmodels = $collection_object->models;
      if ($collection_cmodels[0] != 'islandora:collectionCModel') {
        drush_set_error('OBJECT_IS_NOT_COLLECTION',
          dt('The specified collection object (!collection) does not have a content model of islandora:collectionCModel.',
          array('!collection' => drush_get_option('collection_pid'))));
        drush_user_abort(dt('Exiting, no datastreams fetched.'));
        exit;
      }
    }
  }

  $settings_variables = islandora_get_csv_system_variables();
  $options = array();
  foreach ($settings_variables as $variable) {
    $options[$variable] = variable_get($variable);
  }

  $output_file_path = trim(drush_get_option('output_file'));
  if ($results = islandora_get_csv_query_solr($pid, $options)) {
    islandora_get_csv_write_output($results);
    drush_log(dt('CSV file for metadata in the !pid collection written to !path.', array(
      '!pid' => $pid,
      '!path' => $output_file_path,
    )), 'ok');
  }
  else {
    drush_log(dt('Could not generate CSV for !pid', array(
      '!pid' => $pid,
    )), 'warning');
  }
}
