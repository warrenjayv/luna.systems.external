+ luna.systems.external.console
  - a tcp based console for receiving debug logs

  - ![image](https://user-images.githubusercontent.com/40836157/209758992-5cad81f0-8973-4ef1-8d35-c052f270161a.png)

+ luna.systems.external.debug.client (interface for unity) 
  - usage
    ```csharp
    using System.Collections;
    using System.Collections.Generic;
    using UnityEngine;
    using UnityEngine.UI;

    public class lw_console_ui : MonoBehaviour
    {
      public InputField user_input;

      public void read_user_input ( ) {
        switch (user_input.text) {
            case "list ip":
                luna.external.debug.client.list_ip_addresses();
                break;
            case "set ip":
                luna.external.debug.client.select_ip_index(); 
                break;
            case "test": 
                luna.external.debug.client.execute_client("testing console."); 
                break;
        }
      }   
    }
    ```

