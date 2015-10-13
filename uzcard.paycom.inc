<?php

function _uzcard_pay(){
	
	$data = file_get_contents('php://input');		
	$data = json_decode($data);

	// $PAYCOM_PASSWORD='vG#H?dDXNTfQSmE8@K@c?AHVm?vd67h%TdU&';
	$PAYCOM_PASSWORD= variable_get('paycom_password','');	
	
	function parseRequestHeaders() {
	    $headers = array();
	    foreach($_SERVER as $key => $value) {
	        if (substr($key, 0, 5) <> 'HTTP_') {
	            continue;
	        }
	        $header = str_replace(' ', '-', ucwords(str_replace('_', ' ', strtolower(substr($key, 5)))));
	        $headers[$header] = $value;
	    }
	    return $headers;
	}


	function RPCErrors($method){
		$error=array();
		switch ($method) {

		    case 'transport_error':
		        $error=array(
		            'code' => -32300,
		            'message' => 'Transport Error',
		            'data' => "mirandauniq"
		        );
   				break;
		    case 'access_deniet':
		        $error=array(
		            'code' => -32504,
		            'message' => 'AccessDeniet',
		            'data' => "mirandauniq"
		        );		    
		    	break;    	
		    case 'parse_error':
		        $error=array(
		            'code' => -32700,
		            'message' => 'Parse Error',
		            'data' => "mirandauniq"
		        );
		        break;
		    case 'method_not_found': {
		        $error=array(
		            'code' => -32601,
		            'message' => 'Method not found',
		            'data' => "mirandauniq"
		        );
   				break;
		    }
		} 

		return $error;  
	}

	function BilingErrors($method){
		
		$error=array();

		switch($method){

		    case 'transaction_not_found': 
		        $error=array(
		            'code' => -31003,
		            'message' => 
		            array(
		            	'ru' => 'Транзакция не найденна',
		            	'uz' => 'Транзакция не найденна',
		            	'en' => 'Транзакция не найденна'
		            	),
		            'data' => null
		        );	    
   				break;

		    case 'unexpected_transaction_state': 
		        $error=array(
		            'code' => -31008,
		            'message' => array(
		            	'ru'=>'Статус транзакции не позволяет выполнить операцию',
		            	'uz' => 'Статус транзакции не позволяет выполнить операцию',
		            	'en' => 'Статус транзакции не позволяет выполнить операцию'
		            	),
		            'data' => null
		        );
   				break;
		    case 'incorrect_amount': 
		        $error=array(
		            'code' => -31001,
		            'message' => array(
		            	'ru'=>'Неверная сумма заказа',
		            	'uz' => 'Неверная сумма заказа',
		            	'en' => 'Неверная сумма заказа'
		            	),

		            'data' => null
		        );
   				break;
		    case 'order_not_found':
		        $error=array(
		            'code' => -31050,
		            'message' => array(
		            	'ru'=>'Заказ не найден',
		            	'uz' => 'Заказ не найден',
		            	'en' => 'Заказ не найден'
		            	),
		            'data' => 'order'
		        );
   				break;

		    case 'order_available': 
		        $error=array(
		            'code' => -31051,
		            'message' => array(
		            	'ru'=>'Не возможно выполнить операцию. Заказ ожидает оплаты или оплачен.',
		            	'uz' => 'Не возможно выполнить операцию. Заказ ожидает оплаты или оплачен.',
		            	'en' => 'Не возможно выполнить операцию. Заказ ожидает оплаты или оплачен.'
		            	),
		            'data' => 'order'
		        );
   				break;

		    case 'order_not_canceled': 
		        $error=array(
		            'code' => -31007,
		            'message' => array(
		            	'ru'=>'Заказ полность выполнен и не подлежит отмене.',
		            	'uz' => 'Заказ полность выполнен и не подлежит отмене.',
		            	'en' => 'Заказ полность выполнен и не подлежит отмене.'
		            	),
		            'data' => null
		        );
				break;		    
		}
		return $error;
	}
	
 	
	function get_order_status_index($order_id){
		
		$commerce_order = commerce_order_load($order_id);
		if($commerce_order){
			switch ($commerce_order->status) {
				case 'checkout_payment':
					return 0;
					break;
				case 'blocked':
					return 1;
					break;
				case 'completed':
					return 2;
					break;
				case 'canceled':
					return -1;
					break;	
			}
		} 
		else return false;
	}
	
	function load_transaction_by_property($field_name, $field_value, $order_by_field='transaction_id'){
        
        //load transaction entity
        $query = new EntityFieldQuery;
       
        $transaction_entity = $query
        ->entityCondition('entity_type', 'commerce_payment_transaction')
        ->propertyCondition($field_name, $field_value)
        ->propertyOrderBy($order_by_field, 'DESC')
        ->range(0, 1)
        ->execute();
        
        //Если в базе существует транзакция
        if(!empty($transaction_entity)) {
        	
        	$transaction_id = current(array_keys($transaction_entity['commerce_payment_transaction']));
        	$transaction = commerce_payment_transaction_load($transaction_id);

        	return $transaction;
        }
        
        watchdog(basename(__FILE__, '.module'), t('Список сущности типа commerce_payment_transaction при ID заказа %order_id - пусто.'), array('%order_id' => $order_id), WATCHDOG_ERROR);
        
        return false;

	}

	/**
	 * Create a transaction and associate it with an order.
	 */
	function set_transaction_status($order_id, $status_code, $paycom_transaction_id, $reason = null) {
	 	
	 	$order = commerce_order_load($order_id);
		$order_wrapper = entity_metadata_wrapper('commerce_order', $order);
	 
		// If this is authorization for which we've already
		// created a transaction...
		$transactions = commerce_payment_transaction_load_multiple(array(), array('order_id' => $order->order_id));
		if (!empty($transactions)) {
			// Load transaction and update that with the capture values.
			$transaction = reset($transactions);
		}
		else {
			// Create a new payment transaction for the order.
			$transaction = commerce_payment_transaction_new('uzcard_payment', $order->order_id);
			$transaction->instance_id = $order->data['payment_method'];
		}
		$transaction->amount = $order_wrapper->commerce_order_total->amount->value();
		$transaction->currency_code = $order_wrapper->commerce_order_total->currency_code->value();
		$transaction->remote_id = $paycom_transaction_id;
		$transaction->remote_status = $status_code;
	 
		// Set a status for the payment - one of COMMERCE_PAYMENT_STATUS_SUCCESS, COMMERCE_PAYMENT_STATUS_PENDING or COMMERCE_PAYMENT_STATUS_FAILURE.
		switch ($status_code) {
			case '1':
				$transaction->payload=array('perform_time' => 0, 'cancel_time' => 0, 'reason' => $reason);
				$transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;				
				$transaction->message = t('The payment has pending.');
				commerce_order_status_update($order, 'blocked');
				break;
			case '2':
				$transaction->payload['perform_time'] = time();
				$transaction->payload['reason'] = $reason;
				$transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
				$transaction->remote_status = $status_code;
				$transaction->message = t('The payment has been successfully processed.');				
				commerce_checkout_complete($order);			
				commerce_order_status_update($order, 'completed');
				break;
			case '-1':
				$transaction->payload['cancel_time'] = time();
				$transaction->payload['reason'] = $reason;
				$transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
				$transaction->remote_status = $status_code;
				$transaction->message = t('The payment has canceled.');
				commerce_order_status_update($order, 'checkout_payment');
				break;
			case '-2':
				$transaction->payload['cancel_time'] = time();
				$transaction->payload['reason'] = $reason;
				$transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
				$transaction->remote_status = $status_code;
				$transaction->message = t('The payment has canceled.');
				commerce_order_status_update($order, 'canceled');
				break;
		}
		commerce_payment_transaction_save($transaction);
	}


	function send_response($error=array(), $result=array(), $id=null){
		$response=array('jsonrpc'=>'2.0','error'=>$error,'result'=>$result,'id'=>$id);
		drupal_json_output($response);	
		drupal_exit();
	}

	$header=parseRequestHeaders();	
	$auth = null;
	if( !empty($header['Authorization'])) $auth = $header['Authorization'];

	if( !$auth
		|| sizeof($auth=preg_split("/ +/", trim($auth))) !== 2
        || !($auth[0] === "Basic" && $auth[1])
		|| sizeof(($auth=(preg_split("/ *: */", trim(utf8_encode(base64_decode($auth[1]))))))) !== 2
		|| !($auth[0] === 'Paycom' && $auth[1] === $PAYCOM_PASSWORD)
		){		
			return send_response(RPCErrors('access_deniet'), null ,$data->id);
		}


	class Biling{
		
		public $params=array();
		public $request_id='';
		public $transactions=array();

		public function CheckPerformTransaction($params, $callback, $request_id){

		    $order_id = $params->account->mirandauniq; 
		    $commerce_order = commerce_order_load($order_id);

		    // $order_wrapper = entity_metadata_wrapper('commerce_order', $commerce_order);
		    // $order_total = $order_wrapper->commerce_order_total->amount->value();

        	//проверяем существует ли заказ
		    if( !$commerce_order ){
		    	return call_user_func_array($callback, array(BilingErrors('order_not_found'), null, $request_id));
		    }

	        //проверяем доступен ли заказ для оплаты 
// ~
	        if( get_order_status_index($order_id) != 0 ) {
		    	return call_user_func_array($callback, array(BilingErrors('order_available'), null, $request_id));
	        }

	        //сверяем сумму заказа
	        if( $commerce_order->commerce_order_total['und'][0]['amount'] * 100 !== $params->amount ) {
		    	return call_user_func_array($callback, array(BilingErrors('incorrect_amount'), null, $request_id));
	        }

	        call_user_func_array($callback, array(null, array('allow'=>true)));


		} 

		public function CreateTransaction($params, $callback, $request_id){	
    
		    $this->params=$params;
		    $this->request_id=$request_id;

		    $paycom_transaction_id=$params->id;
		    $transaction = load_transaction_by_property('remote_id', $paycom_transaction_id, 'remote_id');


		    if( $transaction ){
	            //если такая транзакция уже есть в базе
	            //проверяем состояние транзакции, 
	            //если транзакция оплачена или отменена, возвращаем ошибку

	            if( (int) $transaction->remote_status !== 1 ) {
		    		return call_user_func_array($callback, array(BilingErrors('unexpected_transaction_state'), null, $request_id));                
	            }
	            
	            //если транзакция находится в начальном состоянии
	            //возвращаем ее результат
	            $result = array(
	            	'state' => (int) $transaction->remote_status,
	            	'create_time' => (int) $transaction->created,
	            	'transaction' => (string) $transaction->transaction_id,
	            	'test' => false
	            	);
				return call_user_func_array($callback, array(null, $result, $request_id));				 
		    }	    

	        // если транзакции в базе нету
	        // проверяем можно ли оплатить заказ   
	    //     $this->CheckPerformTransaction($params, function($error, $result) {
	    //         if( $error ) {
					// // drupal_json_output(array('status'=>false));
					// call_user_func_array('send_response', array($error, $result, $this->request_id));

	    //         } 


	    //         //создаем транзакцию и добовляем ее в базу
	            
	    //         $order_id = $this->params->account->mirandauniq;
	    //         set_transaction_status($order_id, 1, $this->params->id);

		   //      $transaction = load_transaction_by_property('remote_id', $this->params->id, 'remote_id');

		   //      if($transaction){
		            
		   //          $result=array(
		   //          	'state' => (int) $transaction->remote_status,
		   //          	'create_time' => (int) $transaction->created,
		   //          	'transaction' => (string) $transaction->transaction_id
		   //          	);
		   //          $callback='send_response';
					// call_user_func_array($callback, array(null, $result, $this->request_id));	
		   //      }
		   //      else{
		   //      	call_user_func_array('send_response', array(BilingErrors('transaction_not_found'), null, $this->request_id));	
		   //      }


	    //     }, $request_id);
// =====================================================================
		    
            $order_id = $this->params->account->mirandauniq;
		    $commerce_order = commerce_order_load($order_id);

        	//проверяем существует ли заказ
		    if( !$commerce_order ){
		    	return call_user_func_array($callback, array(BilingErrors('order_not_found'), null, $request_id));
		    }

	        //проверяем доступен ли заказ для оплаты 
// ~
	        if( get_order_status_index($order_id) != 0 ) {
		    	return call_user_func_array($callback, array(BilingErrors('order_available'), null, $request_id));
	        }
// ~
	        //сверяем сумму заказа
	        // drupal_json_output(array('status' => gettype($commerce_order->commerce_order_total['und'][0]['amount'])) );	 
	        $amount = (int) $commerce_order->commerce_order_total['und'][0]['amount'];
	        if( $amount * 100 != $this->params->amount ) {
		    	return call_user_func_array($callback, array(BilingErrors('incorrect_amount'), null, $request_id));
	        }

            //создаем транзакцию и добовляем ее в базу   

            set_transaction_status($order_id, 1, $this->params->id);

	        $transaction = load_transaction_by_property('remote_id', $this->params->id,'remote_id');

	        if($transaction){
	            
	            $result=array(
	            	'state' => (int) $transaction->remote_status,
	            	'create_time' => (int) $transaction->created,
	            	'transaction' => (string) $transaction->transaction_id
	            	);

				call_user_func_array('send_response', array(null, $result, $this->request_id));	
	        }
	        else{
	        	call_user_func_array('send_response', array(BilingErrors('transaction_not_found'), null, $this->request_id));	
	        }
// =====================================================================      


		}

		public function PerformTransaction($params, $callback, $request_id){
		    
		    $paycom_transaction_id=$params->id;

	        $transaction = load_transaction_by_property('remote_id', $paycom_transaction_id, 'remote_id');
	        if($transaction){
		        //транзакция не найденна
		        if( !$transaction ) {
					return call_user_func_array($callback, array(BilingErrors('transaction_not_found'),  null, $request_id));	
		        }

//~		        //если транзакция доступна для оплаты
		        if( (int) $transaction->remote_status == 1 ) {

				    $order_id=(int) $transaction->order_id;

				    set_transaction_status($order_id, 2, $params->id);

		        }
		        
		        //если транзакция закрыта
//~
		        if( (int) $transaction->remote_status == 2 ) {
		            $result = array(
		            	'state' => (int) $transaction->remote_status,
		            	'perform_time' => (int) $transaction->payload['perform_time'],
		            	'transaction' =>(string) $transaction->transaction_id,
		            	 );

					return call_user_func_array($callback, array(null,  $result, $request_id));	
		        }

				call_user_func_array($callback, array(BilingErrors('unexpected_transaction_state'),  null, $request_id));	
	        }
	        else{
	        	call_user_func_array('send_response', array(BilingErrors('transaction_not_found'),  null, $this->request_id));	
	        }


		} 

		public function CancelTransaction($params, $callback, $request_id){
		    
		    $paycom_transaction_id=$params->id;

	        $transaction = load_transaction_by_property('remote_id', $paycom_transaction_id, 'remote_id');

	        if( !$transaction ) {
				return call_user_func_array($callback, array(BilingErrors('transaction_not_found'), null,$request_id));	
	        }

	        //если транзакция еще не закрыта
			
			$order_id = (int) $transaction->order_id;
	        if( (int) $transaction->remote_status === 1 ) {
	            //отменяем транзакцию
	            //освобождаем заказ

			    set_transaction_status($order_id, -1, $params->id, $params->reason);

	        }

	        //если транзакция уже закрыта
	        if( (int) $transaction->remote_status === 2 ) {

	            //если заказ уже выполнен в полной мере и не подлежит отмене
	            //если бизнес процесы позволяют отменить заказ в любой момент времени 
	            //данную часть логики можно опустить
	            // $commerce_order = commerce_order_load($order_id);

	        	// drupal_json_output(array('status' => 'fail'));
	        	
	            if( get_order_status_index($order_id) == 3 ) {
					return call_user_func_array($callback, array(BilingErrors('order_not_canceled'), null, $request_id));
	            }

	            //если заказ еще возможно отменить
	            if( get_order_status_index($order_id) == 2 ) {

			    	set_transaction_status($order_id, -2, $params->id, $params->reason);
				// return call_user_func_array($callback, array(BilingErrors('order_not_canceled'), null, $request_id));
	                //если бизнес процесс позволяет снова оплатить заказ после отмены
	                //$order->state = 0;
	            }
	        }

            $result = array(
            	'state' => (int) $transaction->remote_status,
            	'cancel_time' => (int) $transaction->payload['cancel_time'],
            	'transaction' =>(string) $transaction->transaction_id,
            	 );
			call_user_func_array($callback, array(null, $result, $request_id));	
		} 

		public function CheckTransaction($params, $callback, $request_id){

		    $paycom_transaction_id=$params->id;

	        $transaction = load_transaction_by_property('remote_id', $paycom_transaction_id, 'remote_id');	

	        //транзакция не найденна
	        if( !$transaction ) {	            
	            return	call_user_func_array($callback, array(BilingErrors('transaction_not_found'),  null, $request_id));	
	        }
            $result = array(
            	'state' => (int) $transaction->remote_status,
            	'create_time' => (int) $transaction->created,            	
            	'perform_time' => (int) $transaction->payload['perform_time'],
            	'cancel_time' => (int) $transaction->payload['cancel_time'],
            	'transaction' =>(string) $transaction->transaction_id,
	            'reason' => $transaction->payload['reason']
            	 );
			return call_user_func_array($callback, array(null,  $result, $request_id));		

		}
	}

	// // ------------------------------------------------

	$biling = new Biling();
	$request_id='';

    //получаем POST Данные
	function get_post_data($data, $object, $request_id){
        try {
            $request_id = $data->id;
        }
        catch(Exception $e) {
            return send_response(RPCErrors('parse_error'), null, $request_id);
        }

        $method = $data->method;

        if( !$object->$method($data->params, 'send_response', $request_id) ) {
            return send_response(RPCErrors('method_not_found'), null, $request_id);
        }

        $object->$method($data->params, 'send_response', $request_id); 
  
	}

	get_post_data($data, $biling, $request_id);

}
