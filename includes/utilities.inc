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
 * @param array $options
 *   The $form_state['values'] from the generate CSV form.
 *
 * @return array
 *   The CSV output from Solr, or FALSE if there was a problem.
 */
function islandora_get_csv_query_solr($pid, $options) {
  $collection_pid = preg_replace('/:/', '\:', $pid);
  $collection_limiter = $options['islandora_get_csv_collection_limiter'];
  $field_type_filter = $options['islandora_get_csv_field_suffix_list'];
  $field_type_filter = preg_replace('/,/', '|', $field_type_filter);
  $field_name_pattern = $options['islandora_get_csv_field_pattern'];

  // We don't use IslandoraSolrQueryProcessor for CSV results because
  // it doesn't seem possible to override the 'wt' parameter.
  $solr_endpoint = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $protocol =  variable_get('islandora_get_csv_protocol', 'http://');
  $solr_request = $protocol . $solr_endpoint . "/select?q=*:*&wt=csv&rows=0&fl=" . $field_name_pattern;
  $field_list = @file_get_contents($solr_request);
  $field_array = explode(',', $field_list);
  $field_array_filtered = array();
  foreach ($field_array as $fieldname) {
    if (preg_match('/(' . $field_type_filter . ')$/', $fieldname)) {
      $field_array_filtered[] = $fieldname;
    }
  }

  if ($collection_limiter == 'ancestors') {
    if (!islandora_get_csv_check_for_ancesors($pid)) {
      drupal_set_message(t('Your Solr index does not include ancestors.'), 'warning');
      return FALSE;
    }
  }

  // Filter out unwanted fields.
  $unwanted_fields_string = $options['islandora_get_csv_omit_fields'];
  $unwanted_fields_array = explode(',', $unwanted_fields_string);
  $field_array_filtered = array_diff($field_array_filtered, $unwanted_fields_array);

  // Remove _x eqivalents to _mxx fields.
  if ($options['islandora_get_csv_remove_single_value_fields']) {
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
  $additional_fields = $options['islandora_get_csv_additional_fields'];
  $fields_used_in_collection = explode(',', $additional_fields);

  // Ancestors use the literal PID, not the one prepended by 'info:fedora'.
  if ($collection_limiter == 'ancestors' && $options['islandora_get_csv_include_descendents']) {
    // $fq = 'ancestors_ms:"' . $pid . '"';
    $collection_filter = 'ancestors_ms:"' . $pid . '"';
  }
  else {
    $collection_field = $options['islandora_get_csv_collection_membership'];
    // $fq = $collection_field . ':"info\:fedora/' . $collection_pid . '"';
    $collection_filter = urlencode($collection_field . ':"info\:fedora/' . $collection_pid . '"');
  }

  foreach ($field_array_filtered as $filtered_field_name) {
    $field_request = $protocol . $solr_endpoint . '/select?q=' . $filtered_field_name . ':*&wt=json&rows=0&fq=' . $collection_filter;
    $field_response = @file_get_contents($field_request);
    $field_response = json_decode($field_response, TRUE);

    if ($field_response['response']['numFound'] > 0) {
      $fields_used_in_collection[] = $filtered_field_name;  
    } 
  }
  $collection_fields_param = implode(',', $fields_used_in_collection);
 
  $solr_request = $protocol . $solr_endpoint . '/select?q=PID:*&wt=csv&rows=1000000&fq=' . $collection_filter . '&fl=' . $collection_fields_param;
  if (strpos($solr_request, 'http://http://') !== false || strpos($solr_request, 'https://https://') !== false || strpos($solr_request, 'http://https://') !== false || strpos($solr_request, 'https://http://') !== false) {
    drupal_set_message(t('Your Solr endpoint already contains the %protocol protocol.
    Please configure islandora_get_csv and set Protocol to "None".', array('%protocol' => $protocol)), 'error');
  }
  elseif (strpos($solr_request, 'http') !== 0) {
    drupal_set_message(t('Your Solr endpoint lacks a protocol. Please configure islandora_get_csv 
    and set Protocol to "http" or "https".'), 'error');
  }
  $metadata_csv = @file_get_contents($solr_request);

  if ($options['islandora_get_csv_show_query']) {
    drupal_set_message(t('@query', array('@query' => $solr_request)));
  }

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

/**
 * Gets the raw Solr document for a random member of the collection.
 *
 * @param string $collection_pid
 *
 * @return string
 */
function islandora_get_csv_get_sample_solr_doc($collection_pid) {
  // First, get a random PID from the current collection.
  $collection_pid = preg_replace('/:/', '\:', $collection_pid);
  $collection_limiter = variable_get('islandora_get_csv_collection_limiter', 'is member of');

  if ($collection_limiter == 'is member of') {
    $collection_field = variable_get('islandora_get_csv_collection_membership', 'RELS_EXT_isMemberOfCollection_uri_t');
    // $fq = $collection_field . ':"info\:fedora/' . $collection_pid . '"';
    $collection_filter = urlencode($collection_field . ':"info\:fedora/' . $collection_pid . '"');
  }
  if ($collection_limiter == 'ancestors') {
    // $fq = 'ancestors_ms:"' . $collection_pid . '"';
    $collection_filter = 'ancestors_ms:"' . $collection_pid . '"';
  }

  $solr_endpoint = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $protocol =  variable_get('islandora_get_csv_protocol', 'http://');
  $solr_request = $protocol . $solr_endpoint . '/select?q=PID:*&wt=json&rows=1000000&fq=' . $collection_filter . '&fl=PID';
  $response = @file_get_contents($solr_request);
  $response = json_decode($response, TRUE);
  $num_docs = count($response['response']['docs']);
  $random_index = rand(0, $num_docs);
  $random_pid = $response['response']['docs'][$random_index]['PID'];

  // Then, get the raw Solr document for the random PID.
  $random_pid =  preg_replace('/:/', '\:', $random_pid);
  $solr_request = $protocol . $solr_endpoint . '/select?q=PID:' . $random_pid . '&wt=json&rows=1';
  // This is a hack.... re-encode the JSON so we can pretty print it.
  $response = @file_get_contents($solr_request);
  $response = json_decode($response, TRUE);
  $pretty_doc = json_encode($response['response']['docs'][0], JSON_PRETTY_PRINT);
  return '<pre>' . $pretty_doc . '</pre>';
}


/**
 * Checks to see if 'ancestors' have been indexed.
 *
 * @param string $collection_pid
 *   The raw (including ':') PID of the current collection.
 *
 * @return bool
 *   TRUE if ancestors are present, FALSE if not.
 */
function islandora_get_csv_check_for_ancesors($collection_pid) {
  $collection_pid = preg_replace('/:/', '\:', $collection_pid);
  $protocol =  variable_get('islandora_get_csv_protocol', 'http://');
  $solr_endpoint = variable_get('islandora_solr_url', 'localhost:8080/solr');
  $ancestors_request = $protocol . $solr_endpoint . '/select?q=ancestors_ms:*&fl=PID&wt=json&rows=1&fq=ancestors_ms:"' . $collection_pid . '"';
  $ancestors_response = @file_get_contents($ancestors_request);
  $ancestors_response = json_decode($ancestors_response, TRUE);
  if ($ancestors_response['response']['numFound'] > 0) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/**
 * List of system variables used by this module.
 *
 * @return array
 */
function islandora_get_csv_system_variables() {
  $variables = array(
    'islandora_get_csv_additional_fields',
    'islandora_get_csv_collection_limiter',
    'islandora_get_csv_collection_membership',
    'islandora_get_csv_field_pattern',
    'islandora_get_csv_field_suffix_list',
    'islandora_get_csv_omit_fields',
    'islandora_get_csv_protocol',
    'islandora_get_csv_remove_single_value_fields',
  );
  return $variables;
}
