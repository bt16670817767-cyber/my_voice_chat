using System.Collections;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

namespace Managers
{
    public class ConnectionHandler : MonoBehaviour
    {
        [Header("输入框")]
        public TMP_InputField usernameInput;
        public TMP_InputField passwordInput;
        
        [Header("反馈")]
        public TextMeshProUGUI feedbackText;
        public Button connectButton;
        
        [Header("有效账号（演示用）")]
        public string validUsername = "demo";
        public string validPassword = "123456";

        void Start()
        {
            // 启动时清空反馈
            if (feedbackText != null)
                feedbackText.text = "";
        }

        // CONNECT 按钮调用这个方法（替代直接 ShowChatMenu）
        public void OnConnectClicked()
        {
            string username = usernameInput != null ? usernameInput.text : "";
            string password = passwordInput != null ? passwordInput.text : "";

            // 空检查
            if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
            {
                ShowFeedback("Please enter username and password", Color.yellow);
                return;
            }

            // 假验证：disable 按钮，模拟"连接中"
            if (connectButton != null) connectButton.interactable = false;
            ShowFeedback("Connecting...", Color.white);

            StartCoroutine(SimulateLogin(username, password));
        }

        IEnumerator SimulateLogin(string username, string password)
        {
            // 模拟连接服务器的延迟
            yield return new WaitForSeconds(1f);

            if (username == validUsername && password == validPassword)
            {
                ShowFeedback("Login successful!", Color.green);
                yield return new WaitForSeconds(0.5f);
                
                // 跳转
                if (MenuManager.Instance != null)
                {
                    MenuManager.Instance.ShowChatMenu();
                }
            }
            else
            {
                ShowFeedback("Invalid username or password", new Color(1f, 0.4f, 0.4f));
                if (connectButton != null) connectButton.interactable = true;
            }
        }
        public void ClearInputs()
		{
   			 if (usernameInput != null) usernameInput.text = "";
    		 if (passwordInput != null) passwordInput.text = "";
    		 if (feedbackText != null) feedbackText.text = "";
   			 if (connectButton != null) connectButton.interactable = true;
		}

        void ShowFeedback(string message, Color color)
        {
            if (feedbackText != null)
            {
                feedbackText.text = message;
                feedbackText.color = color;
            }
        }
        
    }
}