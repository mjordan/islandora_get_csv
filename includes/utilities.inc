<?php
/**
 * @file
 * Utility functions.
 */

/**
 * Queries the Solr index to get our list.
 *
 * @param string $pid
 *   The collection's PID.
 * @param string $collection_limiter
 *   Either 'is member of' or 'ancestors'.
 *
 * @return array
 *   The CSV output from Solr, or FALSE if there was a problem.
 */
function islandora_get_csv_query_solr($pid, $collection_limiter) {
  $collection_pid = preg_replace('/:/', '\:', $pid);
  $field_type_filter = variable_get('islandora_get_csv_field_suffix_list', '_s|_ms');
  $field_type_filter = preg_replace('/,/', '|', $field_type_filter);
  $field_name_pattern = variable_get('islandora_get_csv_field_pattern', 'mods_*');

  // We don't use IslandoraSolrQueryProcessor for CSV results because
  // it doesn't seem possible to override the 'wt' parameter.
  $solr_endpoint = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $protocol =  variable_get('islandora_get_csv_protocol', 'http');
  $solr_request = $protocol . '://' . $solr_endpoint . "/select?q=*:*&wt=csv&rows=0&fl=" . $field_name_pattern;
  $field_list = @file_get_contents($solr_request);
  $field_array = explode(',', $field_list);
  $field_array_filtered = array();
  foreach ($field_array as $fieldname) {
    if (preg_match('/(' . $field_type_filter . ')$/', $fieldname)) {
      $field_array_filtered[] = $fieldname;
    }
  }

  // If user chose 'Ancestors', check to see if that field has been indexed.
  if ($collection_limiter == 'ancestors') {
    $ancestors_request = $protocol . '://' . $solr_endpoint . '/select?q=ancestors_ms:*&fl=PID&wt=json&rows=1&fq=ancestors_ms:"' . $collection_pid . '"';
    $ancestors_response = @file_get_contents($ancestors_request);
    $ancestors_response = json_decode($ancestors_response, TRUE);
    if ($ancestors_response['response']['numFound'] == 0) {
      drupal_set_message(t('Your Solr index does not include ancestors.'), 'warning');
      return FALSE;
    }
  }

  // Filter out unwanted fields.
  $unwanted_fields_string = variable_get('islandora_get_csv_omit_fields', 'mods_subject_descendants_all_ms');
  $unwanted_fields_array = explode(',', $unwanted_fields_string);
  $field_array_filtered = array_diff($field_array_filtered, $unwanted_fields_array);

  // Remove _x eqivalents to _mxx fields.
  if (variable_get('islandora_get_remove_single_value_fields', TRUE)) {
    $dupes = array();
    foreach ($field_array_filtered as $field_name_to_test) {
      if (preg_match('/_m.{1,3}$/', $field_name_to_test, $matches)) {
        // E.g., _mst, _ms.
        $suffix = $matches[0];
        // Get everything other than the leading _m.
        $suffix_to_dedupe = substr($suffix, 2);
        // Get everything in the metched field name other than the _mxx.
        $field_name_base = preg_replace('/_m.{1,3}$/', '', $field_name_to_test);
        // Assemble the fieldname we want to remove.
        $field_name_to_remove = $field_name_base . '_' . $suffix_to_dedupe;
        $dupes[] = $field_name_to_remove;
      }
    }
    $field_array_filtered = array_diff($field_array_filtered, $dupes);;
  }

  // Perform a query on every MODS-sourced field in our Solr index using a
  // wildcard and collection-based query filter to see if there are any
  // results. If there are results, the field is used in the collection;
  // if there aren't any results, the field is not used in the collection.
  $additional_fields = variable_get('islandora_get_csv_additional_fields', 'PID');
  $fields_used_in_collection = explode(',', $additional_fields);

  if ($collection_limiter == 'is member of') {
    $fq = 'RELS_EXT_isMemberOfCollection_uri_t:"info\:fedora/' . $collection_pid . '"';
    $collection_filter = 'RELS_EXT_isMemberOfCollection_uri_t:"info\:fedora/' . $collection_pid . '"';
  }
  if ($collection_limiter == 'ancestors') {
    $fq = 'ancestors_ms:"' . $collection_pid . '"';
    $collection_filter = 'ancestors_ms:"' . $collection_pid . '"';
  }

  foreach ($field_array_filtered as $filtered_field_name) {
    $field_request = $protocol . '://' . $solr_endpoint . '/select?q=' . $filtered_field_name . ':*&wt=json&rows=0&fq=' . $collection_filter;
    $field_response = @file_get_contents($field_request);
    $field_response = json_decode($field_response, TRUE);

    if ($field_response['response']['numFound'] > 0) {
      $fields_used_in_collection[] = $filtered_field_name;  
    } 
  }
  $collection_fields_param = implode(',', $fields_used_in_collection);
 
  $solr_request = $protocol . '://' . $solr_endpoint . '/select?q=PID:*&wt=csv&rows=1000000&fq=' . $collection_filter . '&fl=' . $collection_fields_param;
  $metadata_csv = @file_get_contents($solr_request);

  if (strlen($metadata_csv)) {
    return $metadata_csv;
  }
  else {
    return FALSE;
  }
}

/**
 * Writes the CSV results from Solr to the output file.
 *
 * @param 
 */
function islandora_get_csv_write_output($results) {
  $fieldname = drush_get_option('fieldname');
  if (drush_get_option('output_file')) {
    $path = drush_get_option('output_file');
    file_put_contents($path, $results, FILE_APPEND);
  }
}

