<?php
/**
 * @file
 * Provides the widget and other logic to embed a flexinode into a host entity.
 *
 * @author : René Bakx (rene@71media.net)
 * @version 7.1.0-beta-1
 */


/**
 * Implements hook_field_info().
 *
 * Hook in the new flexifield content type
 */

function flexinode_field_info() {
  $fields = array();
  $fields['flexinode'] = array(
    'label' => t('Flexinode'),
    'description' => t('This field internaly embeds the edit form from a flexinode in a host entity'),
    'settings' => array('referenceable_types' => array()),
    'default_widget' => 'flexifield_widget',
    'default_formatter' => 'flexinode_default',
    'property_type' => 'flexinode',
  );
  return $fields;
}

/**
 * Implements hook_schema().
 * Defines storage for flexifield in host entity
 */
function flexinode_field_schema($field) {
  if ($field['type'] == 'flexinode') {
    $columns = array(
      'flid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not NULL' => TRUE,
        'description' => 'Primary key',
      ),
      'vid' => array(
        'type' => 'int',
        'not null' => TRUE,
        'description' => 'Default revision ID.',
      ),
      'type' => array(
        'type' => 'varchar',
        'length' => 128,
        'not NULL' => TRUE,
      )
    );
    return array(
      'columns' => $columns,
      'indexes' => array('flid' => array('flid')),
    );
  }
}

/**
 * Implements hook_field_formatter_info().
 */
function flexinode_field_formatter_info() {
  return array(
    'flexinode_default' => array(
      'label' => t('Default'),
      'field types' => array('flexinode'),
    ),
  );
}

/**
 * Implements hook_widget_info().
 * @return array
 */
function flexinode_field_widget_info() {
  return array(
    'flexinode_field_widget' => array(
      'label' => t('Embedded'),
      'description' => t('Show the entry form for a widget'),
      'field types' => array('flexinode'),
      'settings' => array('referenceable_types' => array()),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_NONE, // no default values for this
      ),
    )
  );
}


/**
 * Implements the hook_fields_settings_form()
 *
 * @param $field
 * @param $instance
 * @param $has_data
 * @return array
 */
function flexinode_field_settings_form($field, $instance, $has_data) {
  return array(
    '#type' => 'markup',
    '#prefix' => t('Flexinode referrences are per instance and can be set on the edit tab'),
  );
}

/**
 * Implements the hook_fields_instance_settings_form().
 *
 * @param $field
 * @param $instance
 * @return array
 */
function flexinode_field_instance_settings_form($field, $instance) {
  if ($field['type'] == 'flexinode') {
    $options = array();
    $available_types = flexinode_get_types();
    foreach ($available_types as $type) {
      $options += array($type->id => check_plain($type->label));
    }
    $settings = $instance['settings'];
    $form = array();
    $form['referenceable_types'] = array(
      '#type' => 'checkboxes',
      '#title' => t('Flexinode bundles that can be selected'),
      '#multiple' => TRUE,
      '#default_value' => (isset($settings['referenceable_types']))
        ? $settings['referenceable_types'] : array(),
      '#options' => $options,
    );
    return $form;
  }
}

/**
 * If the form was not submitted in any kind of way, the initial state of the selector is set as
 * determined by the $items[$delta].
 */
/**
 * Implements hook_field_widget_form()
 *
 * The actually field widget that does most of the hard work. Returns a renderable field element with
 * a container that can or will hold the selected flexinode_type subform.
 * Add's a dropdown selector if a user can select more then one flexinode type in the host entity.
 * Delete's the embedded flexinode if the user changes from one type to another.
 *
 * @param $form
 * @param $form_state
 * @param $field
 * @param $instance
 * @param $langcode
 * @param $items
 * @param $delta
 * @param $element
 * @return array
 */
function flexinode_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  static $flexinode_recursion = 0;

  switch ($instance['widget']['type']) {
    default: // just in case
    case  'flexinode_field_widget':
      if ($flexinode_recursion++ > 4) {
        drupal_set_message(t('The flexinode embedded form is nested to deep, you can only go 4 levels deep!.'), 'error');
        return $element;
      }
      $field_parents = $element['#field_parents'];
      $field_name = $element['#field_name'];
      $language = $element['#language'];
      $parents = array_merge($field_parents, array($field_name, $language, $delta));
      $field_state = field_form_get_state($field_parents, $field_name, $language, $form_state);

      $flexinode_type = NULL;
      $flid = NULL;
      $flexinode_entity = NULL;
      $triggering_element = (isset($form_state['triggering_element'])) ? $form_state['triggering_element'] : NULL;
      $isnew = FALSE;

      // check if the user has set a entity trough the dropdown.
      if (!is_null($triggering_element)) {
        $field_form_submit_state = drupal_array_get_nested_value($form_state['input'], $parents);
        $flexinode_entity = isset($field_state['entity'][$delta]) ? $field_state['entity'][$delta] : NULL;
        $flexinode_type = isset($field_form_submit_state['flexinode_type']) ? $field_form_submit_state['flexinode_type'] : NULL;
        unset($items[$delta]['flid']);
        if (!is_object($flexinode_entity)) {
          $items[$delta]['type'] = $flexinode_type;
        }
        else {
          $items[$delta]['type'] = $flexinode_type;
          $items[$delta]['flid'] = NULL;
        }

        // delete if applicable
        if (is_null($flexinode_type) && $flexinode_type != $flexinode_entity->type) {
          unset($field_state['entity'][$delta]);
          $flexinode_entity->delete();
        }
        // attached the new type the items stack.
        flexinode_field_get_entity($items[$delta], TRUE);
      }
      else {
        if (isset($field_state['entity'][$delta])) {
          $items[$delta]['entity'] = $field_state['entity'][$delta];
        }
      }

      if (!isset($items[$delta])) {
        $field_state = field_form_get_state($field_parents, $field_name, $language, $form_state);
        if ($field_state['items_count'] > 0) {
          $field_state['items_count']--;
        }
        field_form_set_state($field_parents, $field_name, $language, $form_state, $field_state);
        return FALSE;
      }

      $embedded_name = str_replace('_', '-', (implode('-', $parents) . '_form'));
      // build the selector
      $element += array(
        '#prefix' => "<div id='{$embedded_name}'>",
        '#postfix' => "</div>",
        '#type' => 'container',
        '#element_validate' => array('flexinode_validate_element'),
        '#parents' => $parents,
        '#language' => $langcode,
      );

      $available_types = flexinode_get_flexinodetypes($instance);
      $total_available_items = count($available_types);
      $default_value = (isset($items[$delta]['type'])) ? $items[$delta]['type'] : NULL;
      array_push($parents, 'flexinode_type');
      // 2 items == one field + a select <none>
      // check if the dropdown for this one is changed as well!
      if ($total_available_items > 2) {
        $element['flexinode_type'] = array(
          '#weight' => -999,
          '#title' => t('Selected type'),
          '#type' => 'select',
          '#delta' => $delta,
          '#parents' => $parents,
          '#tree' => TRUE,
          '#language' => $langcode,
          '#required' => FALSE,
          '#options' => $available_types,
          '#default_value' => $default_value,
          '#limit_validation_errors' => array(),
          '#flexinode_wrapper' => $embedded_name,
          '#ajax' => array(
            'callback' => 'flexinode_field_widget_ajax_select',
            'effect' => 'fade',
            'progress' => array('type' => 'throbber'),
          )
        );
      }
      elseif ($total_available_items == 2) {
        $items[$delta]['type'] = key(array_reverse($available_types, TRUE));
        $element['flexinode_type'] = array(
          '#type' => "hidden",
          '#default_value' => $items[$delta]['type'],
          '#value' => $items[$delta]['type'],
          '#delta' => $delta,
          '#parents' => $parents,
          '#tree' => TRUE,
          '#language' => $langcode,
        );
      }
      /** attach the embedded entity edit form */
      flexinode_field_get_entity($items[$delta], TRUE);
      $flexinode_entity = $items[$delta]['entity'];
      $element['flexinode_type']['#value'] = $items[$delta]['type'];
      if (is_object($flexinode_entity)) {
        $field_state['entity'][$delta] = $flexinode_entity;
        field_attach_form('flexinode', $flexinode_entity, $element, $form_state, $language);
        field_form_set_state($field_parents, $field_name, $language, $form_state, $field_state);
      }

      // add a delete button if needed.
      if (!is_null($items[$delta]['type'])) {
        $add_more_wrapper_id = str_replace('_', '-', implode('-', array_merge($form['#parents'], array($field_name))) . '-add-more-wrapper');
        $delete = array(
          'delete_button' => array(
            '#weight' => 100,
            '#name' => implode($parents, '_') . '_delete_item',
            '#type' => 'submit',
            '#value' => t('delete this item'),
            '#flexinode_wrapper' => $add_more_wrapper_id,
            '#limit_validation_errors' => array(),
            '#attributes' => array('class' => array('field-flexinode-delete')),
            '#submit' => array('flexinode_field_widget_remove_submit'),
            '#ajax' => array(
              'callback' => 'flexinode_field_widget_ajax_delete',
              'effect' => 'fade',
              'progress' => array('type' => 'throbber'),
              'wrapper' => $add_more_wrapper_id,
            ),
          )
        );
        $element += $delete;
      }
      $flexinode_recursion--;
      return $element;
      break;
  }
}

/**
 * The user changed the flexinode selector.
 * @param $form
 * @param $form_state
 * @return array|null
 */
function flexinode_field_widget_ajax_select($form, &$form_state) {
  return flexinode_field_ajax_response($form, $form_state, -1, TRUE);
}

/**
 * The user presed the 'delete this item' button.
 * @param $form
 * @param $form_state
 * @return array|NULL
 */
function flexinode_field_widget_ajax_delete($form, &$form_state) {
  return flexinode_field_ajax_response($form, $form_state, -2);
}

/**
 * Finds the subform based on the triggering_element and the given offset.
 *
 * Then digg trough the ajax_html_ids[] to see if we can find the actual html id,
 * as this CAN be changed by drupal_html_id() by click on the 'add another item' button'
 * The advantage is that newer elements have the tendency to float to the top
 * so the first match should be the one. @see drupal_html_id();
 *
 * Return the rendered form packeged as a json AHAH response.
 *
 * @param $form array the complete form where the subform is selected from.
 * @param $form_state array form state by reference
 * @param $offset int negative offset, aka the element to strip of the end of the array_parents array
 * @param $stripweight bool strip the $_weight of a subform.
 * @return array AHAH respsonse
 */
function flexinode_field_ajax_response($form, &$form_state, $offset, $stripweight = FALSE) {
  $button = $form_state['triggering_element'];
  $address = array_slice($form_state['triggering_element']['#array_parents'], 0, $offset);
  $html_id = $button['#flexinode_wrapper'];
  $element = drupal_array_get_nested_value($form, $address);
  if (isset($_POST['ajax_html_ids'])) {
    foreach ($_POST['ajax_html_ids'] as $post_html_id) {
      if (substr_count($post_html_id, $html_id)) {
        $html_id = $post_html_id;
        continue;
      }
    }
  }
  if ($stripweight && isset($element['_weight'])) {
    unset($element['_weight']);
  }
  $return = array(
    '#type' => 'ajax',
    '#commands' => array(
      ajax_command_replace('#' . $html_id, drupal_render($element))
    ),
  );
  $messages = theme('status_messages');
  if ($messages) {
    $return['#commands'][] = ajax_command_prepend('#' . $html_id, $messages);
  }
  return $return;
}

/**
 * Delete a field from a form, inspired by the Field_Collection module.
 *
 * @param $form
 * @param $form_state
 */
function flexinode_field_widget_remove_submit($form, &$form_state) {
  $button = $form_state['triggering_element'];
  // Where in the form we'll find the parent element.
  $address = array_slice($button['#array_parents'], 0, -1);
  // Go one level up in the form, to the widgets container.
  $parent_element = drupal_array_get_nested_value($form, $address);
  // Delete the entity from the database
  $flexinode = $parent_element['#entity'];
  if (is_numeric($flexinode->flid)) {
    $flexinode->delete();
  }
  // keep the delta from the button, but remove it from the adress to get the entire tree.
  $delta = array_pop($address);

  $field_name = $parent_element['#field_name'];
  $langcode = $parent_element['#language'];
  $parents = $parent_element['#field_parents'];

  $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);
  // rebuild the entire tree shifting all elements one up from starting at the deleted one
  for ($i = $delta; $i <= $field_state['items_count']; $i++) {
    $old_element_address = array_merge($address, array($i + 1));
    $new_element_address = array_merge($address, array($i));

    $moving_element = drupal_array_get_nested_value($form, $old_element_address);
    $moving_element_value = drupal_array_get_nested_value($form_state['values'], $old_element_address);
    $moving_element_input = drupal_array_get_nested_value($form_state['input'], $old_element_address);
    $moving_element['#parents'] = $new_element_address;
    form_set_value($moving_element, $moving_element_value, $form_state);
    drupal_array_set_nested_value($form_state['input'], $moving_element['#parents'], $moving_element_input);

    // Move the entity in the saved state.
    if (isset($field_state['entity'][$i + 1])) {
      $field_state['entity'][$i] = $field_state['entity'][$i + 1];
    }
    else {
      unset($field_state['entity'][$i]);
    }
  }
  // update counter.
  if ($field_state['items_count'] > 0) {
    $field_state['items_count']--;
  }
  $input = drupal_array_get_nested_value($form_state['input'], $address);
  // Sort by weight
  uasort($input, '_field_sort_items_helper');

  // Reweight everything in the correct order.
  $weight = -1 * $field_state['items_count'];
  foreach ($input as $key => $item) {
    if ($item) {
      $input[$key]['_weight'] = $weight++;
    }
  }
  drupal_array_set_nested_value($form_state['input'], $address, $input);
  field_form_set_state($parents, $field_name, $langcode, $form_state, $field_state);
  $form_state['rebuild'] = TRUE;
}

/**
 * Implements hook_widget_validate().
 *
 * If the form is submitted, filter out the flexinodes, save them and set
 * the value according to flexinode_field_schema();
 *
 * @param $element
 * @param $form_state
 */
function flexinode_validate_element(&$element, &$form_state) {
  // Form got submitted

  $field_parents = $element['#field_parents'];
  $field_name = $element['#field_name'];
  $language = $element['#language'];
  $flexinode_entity = FALSE;
  $field_state = field_form_get_state($field_parents, $field_name, $language, $form_state);
  if (FALSE !== isset($field_state['entity'][$element['#delta']])) {
    $flexinode_entity = $field_state['entity'][$element['#delta']];
    //field_attach_form_validate('flexinode', $flexinode_entity, $element, $form_state);
  }

  // attach the pre-saved entity to the form.
  if ($form_state['submitted'] && !form_get_errors() && ($form_state['triggering_element']['#parents'][0] != $element['#parents'][0])) {
    $submitted_values = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
    $submitted_type = (isset($submitted_values['flexinode_type'])) ? $submitted_values['flexinode_type'] : NULL;
    // You can only save a entity if it is attached to a form.
    if ($flexinode_entity && !is_null($submitted_type)) {
      field_attach_submit('flexinode', $flexinode_entity, $element, $form_state);
      $flexinode_form_item = drupal_array_get_nested_value($form_state['values'], $element['#parents']);
      // entity is fished out and stored of the submitted values array in flexinode_field_presave
      $flexinode_form_item['entity'] = $flexinode_entity;
      $flexinode_form_item['#weight'] = (isset($element['_weight'])) ? $element['_weight']['#value'] : $element['#weight'];
      form_set_value($element, $flexinode_form_item, $form_state);
    }
  } // End store flexinode
}

/**
 * Implements hook_field_presave();
 *
 * Does the actually saving of the field, and set's the value in items array correct for the form API.
 *
 * @param $host_entity_type
 * @param $host_entity
 * @param $field
 * @param $instance
 * @param $langcode
 * @param $items
 */
function flexinode_field_presave($host_entity_type, $host_entity, $field, $instance, $langcode, &$items) {

  foreach ($items as $delta => &$item) {
    // If the host entity is saved as new revision, do the same for the item.
    if (FALSE === flexinode_field_is_empty($item, $field)) {

      $flexinode = (isset($item['entity'])) ? $item['entity'] : NULL;
      if (is_object($flexinode)) {
        if (!empty($host_entity->revision)) {
          $flexinode->revision = TRUE;
          $is_default = entity_revision_is_default($host_entity_type, $host_entity);
          // If an entity type does not support saving non-default entities,
          // assume it will be saved as default.
          if (!isset($is_default) || $is_default) {
            $flexinode->default_revision = TRUE;
            $flexinode->archived = FALSE;
          }
        }
        $flexinode->save(TRUE);
        $item = array(
          'flid' => $flexinode->flid,
          'type' => $flexinode->type,
          'vid' => $flexinode->vid,
        );
      }
    }
    else {
      unset($items[$delta]);
    }
  }

}

/**
 * Implements hook_is_empty().
 *
 * @param $item
 * @param $field
 * @return bool
 */
function flexinode_field_is_empty($item, $field) {
  if (!isset($item['flexinode_type']) || $item['flexinode_type'] == '') {
    return TRUE;
  }
  if (!isset($item['entity'])) {
    return TRUE;
  }
  return FALSE;
}


/**
 * Implements hook_field_formatter_settings_form()
 *
 * @param $field
 * @param $instance
 * @param $view_mode
 * @param $form
 * @param $form_state
 * @return mixed
 */
function flexinode_field_formatter_settings_form($field, $instance, $view_mode, $form, &$form_state) {
  $display = $instance['display'][$view_mode];
  $referencables = entity_load('flexinode_type', $instance['settings']['referenceable_types']);
  if ($display['type'] == 'flexinode_inherit') {
    foreach ($referencables as $referencable) {
      $modes = field_view_mode_settings('flexinode', $referencable->type);
      $viewmodes = array();
      foreach ($modes as $name => $settings) {
        if ($settings['custom_settings']) {
          $viewmodes[$name] = $name;
        }
      }
      $form['updated'] = array(
        '#type' => 'hidden',
        '#value' => TRUE,
      );
      $form['displaymode'][$view_mode][$referencable->type] = array(
        '#type' => 'select',
        '#options' => $viewmodes,
        '#title' => check_plain($referencable->label),
      );
    }
  }
  return $form;
}


/**
 * Implements hook_field_formatter_settings_summary().
 *
 * And since i could not find another way without creating a shitload of code
 * the above created formatter sneaky inserts a UPDATED into $instance
 * so I could save the settings with field_instance_update();
 *
 * @param $field
 * @param $instance
 * @param $view_mode
 * @return bool|string
 */
function flexinode_field_formatter_settings_summary($field, $instance, $view_mode) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];
  $return = FALSE;
  if ($display['type'] == 'flexinode_inherit') {
    $referencables = entity_load('flexinode_type', $instance['settings']['referenceable_types']);
    foreach ($referencables as $referencable) {
      $lookup[$referencable->type] = $referencable->label;
    }
    if (isset($settings['updated'])) {
      unset($settings['updated']);
      field_update_instance($instance);
    }
    $summary = '';
    // TODO, move to decent theme function ;)
    foreach ($settings['displaymode'][$view_mode] as $flexinode => $mode) {
      $summary .= $lookup[$flexinode] . ' -> ' . $mode . '<br>';
    }
    $return = $summary;
  }
  return $return;
}


/**
 * Implements hook_field_delete().
 *
 * After this field get's removed, remove all the created entities to.
 *
 * @param $entity_type
 * @param $entity
 * @param $field
 * @param $instance
 * @param $langcode
 * @param $items
 */
function flexinode_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  $flids = array();
  foreach ($items as $item) {
    if (isset($item['flid'])) {
      $flids[] = $item['flid'];
    }
  }
  flexinode_delete_multiple($flids);
}

/**
 * Returns available flexinode_types for a given field.
 * @param $field
 * @return array
 */
function flexinode_get_flexinodetypes($field) {
  $name = '_flexinodetype_' . $field['field_name'];
  $available_flexinode_types = drupal_static($name);
  if (!$available_flexinode_types) {
    $available_flexinode_types = array(NULL => t('none'));
    $all_flexinode_types = flexinode_get_types(NULL, TRUE);
    /**
     * @var $flexinode_type FlexinodeType
     */
    foreach ($all_flexinode_types as $id => $flexinode_type) {
      if (in_array($id, array_values($field['settings']['referenceable_types']))) {
        $available_flexinode_types[$flexinode_type->type] = $flexinode_type->label;
      }
    }
    ksort($available_flexinode_types, 6);
    drupal_static($name, $available_flexinode_types);
  }
  return $available_flexinode_types;
}

