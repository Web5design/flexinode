<?php
// $Id: Exp $

/**
 * @file mfx.field.inc
 *
 * Provides the embeded field to be used in fieldable entities.
 *
 * @author : René Bakx (rene@71media.net)
 *
 */


define('FL_NEW', 1);
define('FL_EDIT', 2);
define('FL_EMPTY', 3);


/**
 * Implements hook_field_info().
 *
 * Hook in the new flexifield content type
 * @return array
 */

function mfx_field_info() {
    $fields = array();
    $fields['flexinode'] = array(
        'label' => t('Flexinode'),
        'description' => t('This field internaly embeds the edit form from a flexinode'),
        'settings' => array('referenceable_types' => array()),
        'default_widget' => 'mfx_flexifield_widget',
        'default_formatter' => 'flexinode_default',
        'property_type' => 'flexinode',
    );
    return $fields;
}

function mfx_field_formatter_info() {
    return array(
        'flexinode_default' => array(
            'label' => t('Default'),
            'field types' => array('flexinode'),
        ));
}

/**
 * Implements hook_schema().
 * Defines storage for flexifield in host entity
 * @param $field
 * @return array
 */
function mfx_field_schema($field) {
    if ($field['type'] == 'flexinode') {
        $columns = array(
            'flid' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => TRUE,
            ),
            'type' => array(
                'type' => 'int',
                'unsigned' => TRUE,
                'not null' => FALSE,
            )
        );
        return array(
            'columns' => $columns,
            'indexes' => array('flid' => array('flid')),
        );
    }
}

/**
 * This form sets the parameters on a field level in the host entity.
 */
function mfx_field_settings_form($field, $instance, $has_data) {
    if ($field['type'] == 'flexinode') {
        $options = array();
        $available_types = mfx_flexinode_get_types();
        foreach ($available_types as $type) {
            $options += array($type->id => check_plain($type->label));
        }
        $settings = $field['settings'];
        $form = array();
        $form['referenceable_types'] = array(
            '#type' => 'checkboxes',
            '#title' => t('Content types that can be referenced'),
            '#multiple' => TRUE,
            '#default_value' => is_array($settings['referenceable_types']) ? $settings['referenceable_types']
                    : array(),
            '#options' => $options,
            '#disabled' => $has_data,
        );
        return $form;
    }
}

function mfx_field_is_empty($item, $field) {
    return false;
}

/**
 * Implements hook_widget_info().
 * @return array
 */
function mfx_field_widget_info() {
    return array(
        'mfx_flexifield_widget' => array(
            'label' => t('Embedded'),
            'description' => t('Show the entry form for a widget'),
            'field types' => array('flexinode'),
            'settings' => array(),
            'behaviors' => array(
                'multiple values' => FIELD_BEHAVIOR_DEFAULT,
                'default value' => FIELD_BEHAVIOR_NONE, // no default values for this
            ),
        )
    );
}

/**
 * The actual widget form itself, creates a $delta indexed container with a dropdown to select the
 * flexinode to embed. This container is replaced trough a AJAX call with a new subform
 * containing both the selector and the embedded form for the flexinode based on the $delta of the selector.
 *
 * @param $form array form
 * @param $form_state array form state
 * @param $field array contains all the meta information about the field itself (deleted, locked, settings, storage)
 * @param $instance array instance of current field contains label, widget type, required etc.
 * @param $langcode string current language of the item where this field is used.
 * @param $items array delta indexed assoc. array with the values from the field storage (aka DB)
 * @param $delta $delta string delta of the item this widget is loaded for.
 * @param $element array contains information about the form element for a single widgets instance. More a convience mix of the above.
 * @return array|null
 */
function mfx_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
    $return = NULL;
    if ($instance['widget']['type'] == 'mfx_flexifield_widget') {
        $elementName = 'fc_' . $field['field_name'] . '_' . $delta;
        $form['#flexinodefields'][$field['field_name']] = $field['field_name'];
        //TODO A bit tricky, could go terribly wrong if other elements are using #AJAX submit, so must refactor into something more distinctive.
        $submit = (isset($form_state['triggering_element'])) ? $form_state['triggering_element']["#value"] : false;
        $flexinodetype = null;
        $flexinodeid = null;
        $state = FL_EMPTY; // TODO, figure out if this is really needed at all!
        if (isset($items[$delta]['flid'])) {
            $flexinodeid = $items[$delta]['flid'];
            $flexinodetype = $items[$delta]['type'];
            $state = FL_EDIT;
        }

        if ($submit && $submit != $flexinodetype) {
            $flexinodetype = $form_state['triggering_element']["#value"];
            $flexinodeid = null;
            $state = FL_NEW;
        }


        // get available flexinodes for this field.
        $available_flexinode_types = array(null => t('none'));
        $available_flexinode_types += array_combine(array_values($field['settings']['referenceable_types']),
                                                    array_values(mfx_flexinode_get_types(null, true)));
        // Element container
        $element[$elementName] = array(
            '#field_name' => $elementName,
            '#type' => 'container',
            '#title' => t('flexifield item'),
            '#weight' => $delta,
            '#delta' => $delta,
            '#state' => $state,
            '#flid' => $flexinodeid,
            '#tree' => true,
            '#language' => $langcode,
            '#process' => array('mfx_process_element'), // this is where the subform is attached
            '#element_validate' => array('mfx_validate_element'), // this where the subform get's saved.
        );

        $element[$elementName]['select'] = array(
            '#delta' => $delta,
            '#type' => 'select',
            '#language' => $langcode,
            '#required' => false,
            '#default_value' => $flexinodetype,
            '#options' => $available_flexinode_types,

        );
        $return = $element;
    }
    return $return;
}

/**
 * The AJAX callback, replace's the form['content'] with the one generated
 * in mfx_process_element.
 */
function mfx_flexifield_widget_trigger_subform_ajax($form, &$form_state) {
    $element = $form_state['triggering_element'];
    array_pop($element['#array_parents']);
    $subform = drupal_array_get_nested_value($form, $element['#array_parents']);
    return $subform['content'];
}

/**
 * Generates the embedded form based on the type set in form_state['input'] or
 * on the type if the record is being edited.
 *
 * @param $element array containing the element currently processed
 * @param $form_state array with form_state;
 * @param $form array actually this the duplicate form from $form_state[]
 * @return array
 */
function mfx_process_element($element, &$form_state, $form) {
    $targetElement = 'ajax_' . $element['#field_name'];
    $element['content'] = array(
        '#prefix' => '<div id="' . $targetElement . '">',
        '#suffix' => '</div>',
        '#field_name' => 'content',
        '#delta' => $element['#delta'],
        '#type' => 'container',
        '#tree' => true,
        '#language' => $element['#language'],
        '#parents' => array_merge($element['#parents'], array('content')),
    );
    // add some ajax magic..
    $element['select']['#ajax'] = array(
        'callback' => 'mfx_flexifield_widget_trigger_subform_ajax',
        'wrapper' => $targetElement,
        'method' => 'replace',
        'effect' => 'slide',
        'progress' => array('type' => 'throbber'),
    );
    /**
     * If the element is new, create a new subform for the flexinode requested in the dropdown
     * Or if the element is being edited, create a subform based on the #flid in the $element['#flid']
     *
     * TODO, clean some overhead in detection both here and in the widget code.
     */
    $flexinodetype = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
    $flexinodetype = $element['select']['#options'][$flexinodetype['select']];
    $flexinode = mfx_field_get_entity($element['#flid'], $flexinodetype);
    switch ($element['#state']) {
        case FL_NEW :
        case FL_EDIT:
            field_attach_form('flexinode', $flexinode, $element['content'], $form_state, $element['#language']);
            break;
        case FL_EMPTY:
            // do nothing ;)
            break;
    }
    return $element;
}

/**
 * implements hook_widget_validate().
 *
 * If the form is submitted, filter out the flexinodes, save them and set
 * the value according to mfx_field_schema();
 *
 * TODO Does not validate at this moment.
 *
 */
function mfx_validate_element(&$element, &$form_state) {
    if ($form_state['submitted']) {
        // Process the element, so the correct subform is attached. TODO figure out WHY this is needed :(
        switch ($element['#state']) {
            case FL_NEW :
            case FL_EDIT:
                $flexinodetype = drupal_array_get_nested_value($form_state['input'], $element['#parents']);
                $type_id = $flexinodetype['select'];
                $flexinodetype = $element['select']['#options'][$type_id];
                $flexinode = mfx_field_get_entity($element['#flid'], $flexinodetype);
                if (!is_object($flexinode)) {
                    dpm("should delete something eh...");
                    // most likely the object has been deleted...
                }
                if (!$flid = $flexinode->submit($element['content'], $form_state)) {
                    form_set_error('Flexinode', t('There was a problem storing a new flexinode'));
                } else {
                    array_pop($element['#parents']);
                    form_set_value($element, array('type' => $type_id, 'flid' => $flid), $form_state);
                }
                break;
        }
        // TODO subform validation!
    }
}


/**
 * create or returns the entity for the embedded form.
 */
function mfx_field_get_entity($flid = null, $type = NULL) {
    $return = false;
    if (isset($flid)) {
        $return = mfx_load_flexinode($flid);
    }
    elseif (!isset($item['entity']) && isset($type)) {
        $return = entity_get_controller('flexinode')->create(array('type' => $type));
    }
    return $return;
}


function mfx_field_formatter_view($flexinode_type, $flexinode, $field, $instance, $langcode, $items, $display) {
    dpm($display);

}