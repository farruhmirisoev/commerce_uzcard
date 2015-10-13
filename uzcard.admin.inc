<?php
/**
 * Uzcard admin configuration page
 */
function uzcard_admin() {
  
  $form = array();
  $form['settings']=array(
    '#title' => t('Настройки модуля'),
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['settings']['merchant_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Merchant ID'),    
    '#default_value' => variable_get('merchant_id', ''),
    '#description' => t("Merchant ID"),
    '#required' => TRUE,
  );
  
  $form['settings']['paycom_password'] = array(
    '#type' => 'textfield',
    '#title' => t('Paycom password'),    
    '#default_value' => variable_get('paycom_password', ''),
    '#description' => t("Paycom Merchant password"),
    '#required' => TRUE,
  );
  
  $form['settings']['param_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Parameter name'),    
    '#default_value' => variable_get('param_id', ''),
    '#description' => t("for example order_id."),
    '#required' => TRUE,
  );

  $form['settings']['checkout_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Адрес платёжного шлюза'),    
    '#default_value' => variable_get('checkout_url', 'https://checkout.paycom.uz'),
    '#description' => t("Адрес платёжного шлюза по умолчанию https://checkout.paycom.uz, адресс для теста api: http://checkout.test.paycom.uz"),
    '#required' => TRUE,
  );

  return system_settings_form($form);
}

/**
 *  admin field validate
 */
function uzcard_admin_validate($form, &$form_state) {
  $pass = $form_state['values']['paycom_password'];
  // if (!is_numeric($pass)) {
  //   form_set_error('paycom_password', t('You must enter an integer for the maximum number of links.'));
  // }
  // elseif ($pass <= 0) {
  //   form_set_error('paycom_password', t('Maximum number of links must be positive.'));
  // }
}